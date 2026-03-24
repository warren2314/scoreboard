---
name: Serial port DTR/RTS status display bug
description: Admin panel shows disconnected due to port.set() throwing async error causing reconnect loop
type: project
---

The admin panel displays "Serial port not connected" and "Arduino disconnected" even though the serial port IS working and the Arduino IS responding. This was caused by port.set({ dtr: false, rts: false }) in server.js throwing an async error that propagated to the error event handler, triggering an infinite reconnect loop.

**Why:** The serialport library's port.set() may not be supported or may behave differently on the Pi 5's USB stack with the R4 WiFi's ESP32-S3 USB bridge.

**How to apply:** The port.set() call was removed entirely from server.js. This fix has not yet been deployed to the Pi as of 2026-03-20. Needs redeployment.
