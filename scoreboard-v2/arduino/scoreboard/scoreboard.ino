// Cricket Scoreboard Controller
// Droylsden Cricket Club
// v2.3 - Restored guide-compatible production layout
//
// Serial protocol (57600 baud, '#' or newline terminated):
//   Receive: 4,<batA>,<total>,<batB>,<wickets>,<overs>,<target>#
//   Receive: 5#  (test mode)
//   Send:    OK:<field>=<value>
//   Send:    ERR:<message>
//   Send:    READY
//
// Wiring:
//   Shifter set 1: pins 2(SRCK), 3(SERIN), 4(RCK)
//   Shifter set 2: pins 5(SRCK), 6(SERIN), 7(RCK)
//
// KEY FIX (v2.1): On ARM-based Arduino boards (R4 WiFi), global constructors
// run before hardware is initialised. pinMode() in the ShifterStr constructor
// silently failed, leaving all shift-register pins as INPUT. No data ever
// reached the TPIC6B595 chips. Fix: call begin() from setup().

#include <ShifterStr.h>
#include <stdlib.h>
#include <string.h>

#define LED_PIN LED_BUILTIN
#define SERIAL_BAUD 57600
#define CMD_BUF_SIZE 64
#define HEARTBEAT_MS 2000
#define SHIFT_PULSE_MS 20
#define SET1_DIGITS 9
#define SET2_DIGITS 6

#if (SET1_DIGITS != 9)
#error "SET1_DIGITS must be 9"
#endif

#if (SET2_DIGITS != 6)
#error "SET2_DIGITS must be 6"
#endif

Shifter shifterSet1(SET1_DIGITS, 2, 3, 4);
Shifter shifterSet2(SET2_DIGITS, 5, 6, 7);

char cmdBuf[CMD_BUF_SIZE];
uint8_t cmdLen = 0;
bool discardingCmd = false;

unsigned long lastBlink = 0;
bool ledState = false;

void appendField(char* dest, uint8_t& pos, uint8_t maxLen, const char* field) {
  for (uint8_t i = 0; field[i] != '\0' && pos < maxLen; i++) {
    dest[pos++] = field[i];
  }
  dest[pos] = '\0';
}

void padDisplay(char* dest, uint8_t& pos, uint8_t maxLen) {
  while (pos < maxLen) {
    dest[pos++] = '-';
  }
  dest[pos] = '\0';
}

void buildSet1Display(const char* batA, const char* total, const char* batB, char* displayOut) {
  uint8_t pos = 0;
  appendField(displayOut, pos, SET1_DIGITS, batA);
  appendField(displayOut, pos, SET1_DIGITS, total);
  appendField(displayOut, pos, SET1_DIGITS, batB);
  padDisplay(displayOut, pos, SET1_DIGITS);
}

void buildSet2Display(const char* wickets, const char* overs, const char* target, char* displayOut) {
  uint8_t pos = 0;
  appendField(displayOut, pos, SET2_DIGITS, wickets);
  appendField(displayOut, pos, SET2_DIGITS, overs);
  appendField(displayOut, pos, SET2_DIGITS, target);
  padDisplay(displayOut, pos, SET2_DIGITS);
}

void sendFieldAck(const char* fieldName, const char* value) {
  Serial.print("OK:");
  Serial.print(fieldName);
  Serial.print("=");
  Serial.println(value);
}

void showBootDisplay() {
  char display1[SET1_DIGITS + 1];
  char display2[SET2_DIGITS + 1];
  buildSet1Display("--0", "--0", "--0", display1);
  buildSet2Display("0", "-0", "--0", display2);
  shifterSet1.display(display1);
  shifterSet2.display(display2);
}

bool isValidField(const char* s, uint8_t expectedLen) {
  uint8_t len = strlen(s);
  if (len != expectedLen) return false;
  for (uint8_t i = 0; i < len; i++) {
    if (s[i] != '-' && (s[i] < '0' || s[i] > '9')) return false;
  }
  return true;
}

void processCommand() {
  if (cmdLen == 0) return;

  char* p = cmdBuf;
  int cmdId = atoi(p);

  if (cmdId == 5) {
    char display1[SET1_DIGITS + 1];
    char display2[SET2_DIGITS + 1];
    buildSet1Display("111", "222", "333", display1);
    buildSet2Display("4", "44", "555", display2);
    shifterSet1.display(display1);
    shifterSet2.display(display2);
    Serial.println("OK:test_mode");
    return;
  }

  if (cmdId == 4) {
    p = strchr(p, ',');
    if (!p) { Serial.println("ERR:missing fields"); return; }
    p++;

    char* fields[6];
    for (uint8_t i = 0; i < 6; i++) {
      fields[i] = p;
      if (i < 5) {
        p = strchr(p, ',');
        if (!p) { Serial.println("ERR:missing fields"); return; }
        *p = '\0';
        p++;
      }
    }

    if (!isValidField(fields[0], 3)) { Serial.println("ERR:bad batA"); return; }
    if (!isValidField(fields[1], 3)) { Serial.println("ERR:bad total"); return; }
    if (!isValidField(fields[2], 3)) { Serial.println("ERR:bad batB"); return; }
    if (!isValidField(fields[3], 1)) { Serial.println("ERR:bad wickets"); return; }
    if (!isValidField(fields[4], 2)) { Serial.println("ERR:bad overs"); return; }
    if (!isValidField(fields[5], 3)) { Serial.println("ERR:bad target"); return; }

    char display1[SET1_DIGITS + 1];
    char display2[SET2_DIGITS + 1];
    buildSet1Display(fields[0], fields[1], fields[2], display1);
    buildSet2Display(fields[3], fields[4], fields[5], display2);

    shifterSet1.display(display1);
    shifterSet2.display(display2);

    sendFieldAck("batA", fields[0]);
    sendFieldAck("total", fields[1]);
    sendFieldAck("batB", fields[2]);
    sendFieldAck("wickets", fields[3]);
    sendFieldAck("overs", fields[4]);
    sendFieldAck("target", fields[5]);
    return;
  }

  Serial.print("ERR:unknown cmd ");
  Serial.println(cmdId);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(LED_PIN, OUTPUT);

  shifterSet1.begin();
  shifterSet2.begin();
  shifterSet1.setPulseDelayMs(SHIFT_PULSE_MS);
  shifterSet2.setPulseDelayMs(SHIFT_PULSE_MS);
  showBootDisplay();

  Serial.println("READY");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '#' || c == '\n') {
      if (!discardingCmd && cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        processCommand();
      }
      discardingCmd = false;
      cmdLen = 0;
    } else if (c != '\r') {
      if (discardingCmd) {
        continue;
      }
      if (cmdLen < CMD_BUF_SIZE - 1) {
        cmdBuf[cmdLen++] = c;
      } else {
        Serial.println("ERR:cmd too long");
        discardingCmd = true;
        cmdLen = 0;
      }
    }
  }

  unsigned long now = millis();
  if (now - lastBlink >= HEARTBEAT_MS) {
    lastBlink = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }
}
