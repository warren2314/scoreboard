#!/usr/bin/env python3
"""
Droylsden CC BT Scoreboard
Receives scores from the Play Cricket Scorer App over BLE and forwards
them to the scoreboard Node.js server via HTTP.

Uses the same BLE UUIDs as the original BTScoreboard project so the
Play Cricket Scorer App connects without any changes on the app side.

Play Cricket Scorer App message types:
  OVB<n>      Overs bowled (may include decimal e.g. "12.3")
  B1S<n>      Batsman A score
  B2S<n>      Batsman B score
  BTS<n>/<w>  Batting team score / wickets (e.g. "145/6")
  FTS<n>/<w>  Following/target team score (2nd innings)

All other message types are ignored.
DLS is preserved from whatever was last set via the web admin panel.
"""

import os
import sys
import json
import logging
import urllib.request
import urllib.error
import dbus
import dbus.exceptions
import dbus.mainloop.glib
import dbus.service
from gi.repository import GLib

# --- Logging ---
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [BT] %(levelname)s %(message)s',
    datefmt='%H:%M:%S'
)
log = logging.getLogger(__name__)

# --- Config (set via environment or systemd service file) ---
SCOREBOARD_URL = os.environ.get('SCOREBOARD_URL', 'http://localhost')
ADMIN_TOKEN    = os.environ.get('ADMIN_TOKEN', 'changeme')

# --- BLE UUIDs — must match Play Cricket Scorer App ---
UART_SERVICE_UUID = '5a0d6a15-b664-4304-8530-3a0ec53e5bc1'
UART_RX_CHAR_UUID = 'df531f62-fc0b-40ce-81b2-32a6262ea440'
LOCAL_NAME        = 'BT-Scoreboard'

# --- BlueZ D-Bus interface names ---
BLUEZ_SERVICE_NAME           = 'org.bluez'
DBUS_OM_IFACE                = 'org.freedesktop.DBus.ObjectManager'
DBUS_PROP_IFACE              = 'org.freedesktop.DBus.Properties'
LE_ADVERTISING_MANAGER_IFACE = 'org.bluez.LEAdvertisingManager1'
LE_ADVERTISEMENT_IFACE       = 'org.bluez.LEAdvertisement1'
GATT_MANAGER_IFACE           = 'org.bluez.GattManager1'
GATT_SERVICE_IFACE           = 'org.bluez.GattService1'
GATT_CHRC_IFACE              = 'org.bluez.GattCharacteristic1'

mainloop = None

# Score state — matches the Node.js server field names and formats
state = {
    'batsmanA': '--0',
    'total':    '--0',
    'batsmanB': '--0',
    'target':   '--0',
    'wickets':  '0',
    'overs':    '-0',
    'dls':      '--0',
}


# ---------------------------------------------------------------------------
# Score handling
# ---------------------------------------------------------------------------

def fetch_server_state():
    """Pull the current score from the Node.js server on startup so that
    DLS and anything set via the web admin is preserved when BT updates arrive."""
    try:
        with urllib.request.urlopen(f'{SCOREBOARD_URL}/api/status', timeout=3) as r:
            data = json.loads(r.read())
            for key in state:
                if key in data and data[key]:
                    state[key] = data[key]
            log.info('Restored state from server: %s', state)
    except Exception as e:
        log.warning('Could not fetch server state (using defaults): %s', e)


def send_score():
    """POST the current score state to the Node.js API."""
    body = json.dumps(state).encode()
    req = urllib.request.Request(
        f'{SCOREBOARD_URL}/api/score',
        data=body,
        headers={
            'Content-Type': 'application/json',
            'Authorization': f'Bearer {ADMIN_TOKEN}',
        },
        method='POST'
    )
    try:
        with urllib.request.urlopen(req, timeout=3) as r:
            resp = json.loads(r.read())
            log.info('Score sent: %s', resp.get('message', 'ok'))
    except urllib.error.HTTPError as e:
        log.error('HTTP %d sending score: %s', e.code, e.read().decode())
    except Exception as e:
        log.error('Error sending score: %s', e)


def parse_message(msg):
    """Parse a Play Cricket Scorer App BLE message and update state.
    Returns True if state was updated and should be sent to the board."""
    if len(msg) < 3:
        return False

    msg_type = msg[:3]
    msg_data = msg[3:]

    if msg_type == 'OVB':
        # Overs — may arrive as "12.3" (over.ball), take whole overs only
        overs_val = msg_data.split('.')[0]
        state['overs'] = overs_val.rjust(2, '-')
        log.info('OVB → overs=%s', state['overs'])

    elif msg_type == 'B1S':
        state['batsmanA'] = msg_data.rjust(3, '-')
        log.info('B1S → batsmanA=%s', state['batsmanA'])

    elif msg_type == 'B2S':
        state['batsmanB'] = msg_data.rjust(3, '-')
        log.info('B2S → batsmanB=%s', state['batsmanB'])

    elif msg_type == 'BTS':
        # Format: "145/6" or "145/6 & 23/1" (multiple innings) — take current only
        current = msg_data.split(' &')[0].strip()
        parts = current.split('/')
        if len(parts) == 2:
            state['total'] = parts[0].rjust(3, '-')
            wkts = parts[1].strip()
            # All out (10) shown as blank — scoreboard only has 1 digit
            state['wickets'] = '-' if wkts == '10' else wkts
            log.info('BTS → total=%s wickets=%s', state['total'], state['wickets'])
        else:
            log.warning('BTS unexpected format: %r', msg_data)
            return False

    elif msg_type == 'FTS':
        # Target score (2nd innings)
        current = msg_data.split(' &')[0].strip()
        target_val = current.split('/')[0]
        state['target'] = target_val.rjust(3, '-')
        log.info('FTS → target=%s', state['target'])

    else:
        log.debug('Ignored: %s %r', msg_type, msg_data)
        return False

    return True


# ---------------------------------------------------------------------------
# D-Bus exceptions
# ---------------------------------------------------------------------------

class InvalidArgsException(dbus.exceptions.DBusException):
    _dbus_error_name = 'org.freedesktop.DBus.Error.InvalidArgs'

class NotSupportedException(dbus.exceptions.DBusException):
    _dbus_error_name = 'org.bluez.Error.NotSupported'


# ---------------------------------------------------------------------------
# GATT Application
# ---------------------------------------------------------------------------

class Application(dbus.service.Object):
    def __init__(self, bus):
        self.path = '/'
        self.services = []
        dbus.service.Object.__init__(self, bus, self.path)
        self.add_service(ScoreboardService(bus, 0))

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_service(self, service):
        self.services.append(service)

    @dbus.service.method(DBUS_OM_IFACE, out_signature='a{oa{sa{sv}}}')
    def GetManagedObjects(self):
        response = {}
        for service in self.services:
            response[service.get_path()] = service.get_properties()
            for chrc in service.get_characteristics():
                response[chrc.get_path()] = chrc.get_properties()
        return response


class ScoreboardService(dbus.service.Object):
    PATH_BASE = '/org/bluez/scoreboard/service'

    def __init__(self, bus, index):
        self.path = self.PATH_BASE + str(index)
        self.bus = bus
        self.characteristics = []
        dbus.service.Object.__init__(self, bus, self.path)
        self.add_characteristic(ScoreRxCharacteristic(bus, 0, self))

    def get_properties(self):
        return {
            GATT_SERVICE_IFACE: {
                'UUID': UART_SERVICE_UUID,
                'Primary': dbus.Boolean(True),
                'Characteristics': dbus.Array(
                    [c.get_path() for c in self.characteristics],
                    signature='o'
                )
            }
        }

    def get_path(self):
        return dbus.ObjectPath(self.path)

    def add_characteristic(self, chrc):
        self.characteristics.append(chrc)

    def get_characteristics(self):
        return self.characteristics

    @dbus.service.method(DBUS_PROP_IFACE, in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != GATT_SERVICE_IFACE:
            raise InvalidArgsException()
        return self.get_properties()[GATT_SERVICE_IFACE]


class ScoreRxCharacteristic(dbus.service.Object):
    def __init__(self, bus, index, service):
        self.path = service.path + '/char' + str(index)
        self.bus = bus
        self.service = service
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        return {
            GATT_CHRC_IFACE: {
                'Service':     self.service.get_path(),
                'UUID':        UART_RX_CHAR_UUID,
                'Flags':       dbus.Array(['write', 'write-without-response'], signature='s'),
                'Descriptors': dbus.Array([], signature='o'),
            }
        }

    def get_path(self):
        return dbus.ObjectPath(self.path)

    @dbus.service.method(DBUS_PROP_IFACE, in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != GATT_CHRC_IFACE:
            raise InvalidArgsException()
        return self.get_properties()[GATT_CHRC_IFACE]

    @dbus.service.method(GATT_CHRC_IFACE, in_signature='aya{sv}')
    def WriteValue(self, value, options):
        try:
            msg = bytearray(value).decode('utf-8', errors='replace').strip()
            log.debug('BLE received: %r', msg)
            if parse_message(msg):
                send_score()
        except Exception as e:
            log.error('WriteValue error: %s', e)


# ---------------------------------------------------------------------------
# BLE Advertisement
# ---------------------------------------------------------------------------

class ScoreboardAdvertisement(dbus.service.Object):
    PATH_BASE = '/org/bluez/scoreboard/advertisement'

    def __init__(self, bus, index):
        self.path = self.PATH_BASE + str(index)
        dbus.service.Object.__init__(self, bus, self.path)

    def get_properties(self):
        return {
            LE_ADVERTISEMENT_IFACE: {
                'Type':        'peripheral',
                'ServiceUUIDs': dbus.Array([UART_SERVICE_UUID], signature='s'),
                'LocalName':   dbus.String(LOCAL_NAME),
                'Includes':    dbus.Array(['tx-power'], signature='s'),
            }
        }

    def get_path(self):
        return dbus.ObjectPath(self.path)

    @dbus.service.method(DBUS_PROP_IFACE, in_signature='s', out_signature='a{sv}')
    def GetAll(self, interface):
        if interface != LE_ADVERTISEMENT_IFACE:
            raise InvalidArgsException()
        return self.get_properties()[LE_ADVERTISEMENT_IFACE]

    @dbus.service.method(LE_ADVERTISEMENT_IFACE)
    def Release(self):
        log.info('Advertisement released')


# ---------------------------------------------------------------------------
# Adapter discovery & main
# ---------------------------------------------------------------------------

def find_adapter(bus):
    """Find a BLE adapter supporting both GATT and LE advertising.
    Falls back to GATT-only (Pi 5 may expose them on separate paths)."""
    remote_om = dbus.Interface(bus.get_object(BLUEZ_SERVICE_NAME, '/'), DBUS_OM_IFACE)
    objects = remote_om.GetManagedObjects()

    # Prefer an adapter that has both interfaces
    for path, props in objects.items():
        if GATT_MANAGER_IFACE in props and LE_ADVERTISING_MANAGER_IFACE in props:
            return path

    # Fallback: any adapter with GATT
    for path, props in objects.items():
        if GATT_MANAGER_IFACE in props:
            log.warning('Adapter %s has GATT but no LE advertising', path)
            return path

    return None


def main():
    global mainloop

    log.info('Droylsden CC BT Scoreboard starting')
    log.info('API: %s  Token: %s...', SCOREBOARD_URL, ADMIN_TOKEN[:4])

    fetch_server_state()

    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    bus = dbus.SystemBus()

    adapter_path = find_adapter(bus)
    if not adapter_path:
        log.error('No BLE adapter found — is Bluetooth enabled?')
        sys.exit(1)
    log.info('BLE adapter: %s', adapter_path)

    # Power on, make discoverable and pairable
    adapter_props = dbus.Interface(
        bus.get_object(BLUEZ_SERVICE_NAME, adapter_path),
        DBUS_PROP_IFACE
    )
    adapter_props.Set('org.bluez.Adapter1', 'Powered',      dbus.Boolean(True))
    adapter_props.Set('org.bluez.Adapter1', 'Discoverable', dbus.Boolean(True))
    adapter_props.Set('org.bluez.Adapter1', 'Pairable',     dbus.Boolean(True))

    service_manager = dbus.Interface(
        bus.get_object(BLUEZ_SERVICE_NAME, adapter_path),
        GATT_MANAGER_IFACE
    )
    ad_manager = dbus.Interface(
        bus.get_object(BLUEZ_SERVICE_NAME, adapter_path),
        LE_ADVERTISING_MANAGER_IFACE
    )

    app = Application(bus)
    adv = ScoreboardAdvertisement(bus, 0)
    mainloop = GLib.MainLoop()

    service_manager.RegisterApplication(
        app.get_path(), {},
        reply_handler=lambda: log.info('GATT application registered'),
        error_handler=lambda e: (log.error('GATT registration failed: %s', e), mainloop.quit())
    )
    ad_manager.RegisterAdvertisement(
        adv.get_path(), {},
        reply_handler=lambda: log.info('Advertising as "%s" — waiting for Play Cricket Scorer App', LOCAL_NAME),
        error_handler=lambda e: log.warning('Advertisement registration failed: %s', e)
    )

    try:
        mainloop.run()
    except KeyboardInterrupt:
        log.info('Shutting down')
        adv.Release()


if __name__ == '__main__':
    main()
