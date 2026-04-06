// Dual-chain digit test for Droylsden Cricket Club scoreboard.
//
// Chain 1 (top row, 9 digits): SRCK=D2, SERIN=D3, RCK=D4
//   Digits 0-8: BatA(3) + Total(3) + BatB(3)
//
// Chain 2 (bottom row, 10 digits): SRCK=D5, SERIN=D6, RCK=D7
//   Digits 9-18: Target(3) + Wickets(2) + Overs(2) + DLS(3)
//
// Commands (# or newline terminated):
//   help#
//   walk#              — walks an 8 across all 19 digits
//   clear#             — all digits off
//   delay,20#          — set shift pulse delay in ms
//   digit,3,8#         — set digit 3 to '8'
//   digit,12,-#        — set digit 12 to blank
//   raw,2,254#         — set digit 2 to raw byte value
//   scan,4#            — cycle digit 4 through 0-9
//   alltest#           — show 8 on all 19 digits

#include <stdlib.h>
#include <string.h>

// Uncomment the line below once chain 2 is physically wired up
// #define CHAIN2_CONNECTED

// Chain 1 — top row (9 digits)
#define SRCK1  2
#define SERIN1 3
#define RCK1   4
#define NUM_DIGITS1 9

// Chain 2 — bottom row (10 digits)
#define SRCK2  5
#define SERIN2 6
#define RCK2   7
#define NUM_DIGITS2 10

#define TOTAL_DIGITS (NUM_DIGITS1 + NUM_DIGITS2)
#define SERIAL_BAUD 57600

const byte SEGS[] = {252, 96, 218, 242, 102, 182, 190, 224, 254, 230};
unsigned int pulseDelayMs = 1;  // 1ms is plenty for TPIC6B595 at these cable lengths

byte displayBuf[TOTAL_DIGITS];

char cmdBuf[48];
uint8_t cmdLen = 0;
bool discardingCmd = false;

byte encodeGlyph(char glyph) {
  if (glyph == '-') return 0;
  if (glyph >= '0' && glyph <= '9') return SEGS[glyph - '0'];
  return 0;
}

void shiftByteOn(int srckPin, int serinPin, byte value) {
  for (byte bit = 0; bit < 8; bit++) {
    digitalWrite(srckPin, LOW);
    delay(pulseDelayMs);
    digitalWrite(serinPin, (value & 0x01) ? HIGH : LOW);
    delay(pulseDelayMs);
    digitalWrite(srckPin, HIGH);
    delay(pulseDelayMs);
    value >>= 1;
  }
  digitalWrite(srckPin, LOW);
  delay(pulseDelayMs);
  digitalWrite(serinPin, LOW);
}

void latchOn(int rckPin) {
  delay(pulseDelayMs);
  digitalWrite(rckPin, HIGH);
  delay(pulseDelayMs);
  digitalWrite(rckPin, LOW);
  delay(pulseDelayMs);
}

void refreshChain1() {
  digitalWrite(RCK1, LOW);
  delay(pulseDelayMs);
  for (int i = NUM_DIGITS1 - 1; i >= 0; i--) {
    shiftByteOn(SRCK1, SERIN1, displayBuf[i]);
  }
  latchOn(RCK1);
}

void refreshChain2() {
  digitalWrite(RCK2, LOW);
  delay(pulseDelayMs);
  for (int i = TOTAL_DIGITS - 1; i >= NUM_DIGITS1; i--) {
    shiftByteOn(SRCK2, SERIN2, displayBuf[i]);
  }
  latchOn(RCK2);
}

void refreshDisplay() {
  refreshChain1();
#ifdef CHAIN2_CONNECTED
  refreshChain2();
#endif
}

void clearDisplay() {
  for (int i = 0; i < TOTAL_DIGITS; i++) {
    displayBuf[i] = 0;
  }
  refreshDisplay();
}

void setDigitRaw(int idx, byte rawValue) {
  if (idx >= 0 && idx < TOTAL_DIGITS) {
    displayBuf[idx] = rawValue;
  }
  refreshDisplay();
}

void setDigitGlyph(int idx, char glyph) {
  setDigitRaw(idx, encodeGlyph(glyph));
}

void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    yield();
  }
}

void walkDigits() {
  Serial.println("Walking an 8 across all digits...");
  for (int i = 0; i < TOTAL_DIGITS; i++) {
    clearDisplay();
    Serial.print("digit=");
    Serial.println(i);
    setDigitGlyph(i, '8');
    safeDelay(2000);
  }
  clearDisplay();
}

void allTest() {
  Serial.println("All digits showing 8...");
  for (int i = 0; i < TOTAL_DIGITS; i++) {
    displayBuf[i] = SEGS[8];
  }
  refreshDisplay();
}

void scanDigit(int idx) {
  if (idx < 0 || idx >= TOTAL_DIGITS) {
    Serial.println("ERR:digit out of range");
    return;
  }
  Serial.print("Scanning digit ");
  Serial.println(idx);
  for (char g = '0'; g <= '9'; g++) {
    Serial.print("glyph=");
    Serial.println(g);
    setDigitGlyph(idx, g);
    safeDelay(2000);
  }
  setDigitGlyph(idx, '-');
}

void printHelp() {
  Serial.println("Droylsden CC dual-chain digit test");
  Serial.print("Chain1: SRCK="); Serial.print(SRCK1);
  Serial.print(" SERIN="); Serial.print(SERIN1);
  Serial.print(" RCK="); Serial.print(RCK1);
  Serial.print(" digits="); Serial.println(NUM_DIGITS1);
  Serial.print("Chain2: SRCK="); Serial.print(SRCK2);
  Serial.print(" SERIN="); Serial.print(SERIN2);
  Serial.print(" RCK="); Serial.print(RCK2);
  Serial.print(" digits="); Serial.println(NUM_DIGITS2);
  Serial.print("Total digits="); Serial.println(TOTAL_DIGITS);
  Serial.print("pulseDelayMs="); Serial.println(pulseDelayMs);
  Serial.println("Digit map:");
  Serial.println("  0-2: BatA  3-5: Total  6-8: BatB");
  Serial.println("  9-11: Target  12-13: Wickets  14-15: Overs  16-18: DLS");
  Serial.println("Commands: help#, walk#, clear#, alltest#, delay,<ms>#");
  Serial.println("          digit,<0-18>,<0-9 or ->#");
  Serial.println("          raw,<0-18>,<0-255>#, scan,<0-18>#");
}

void processCommand() {
  if (cmdLen == 0) return;

  if (strcmp(cmdBuf, "help") == 0) {
    printHelp();
    return;
  }

  if (strcmp(cmdBuf, "walk") == 0) {
    walkDigits();
    return;
  }

  if (strcmp(cmdBuf, "clear") == 0) {
    clearDisplay();
    Serial.println("OK:clear");
    return;
  }

  if (strcmp(cmdBuf, "alltest") == 0) {
    allTest();
    Serial.println("OK:alltest");
    return;
  }

  if (strncmp(cmdBuf, "delay,", 6) == 0) {
    int value = atoi(cmdBuf + 6);
    if (value < 0) {
      Serial.println("ERR:bad delay");
      return;
    }
    pulseDelayMs = (unsigned int)value;
    Serial.print("OK:delay=");
    Serial.println(pulseDelayMs);
    return;
  }

  if (strncmp(cmdBuf, "scan,", 5) == 0) {
    int idx = atoi(cmdBuf + 5);
    scanDigit(idx);
    return;
  }

  if (strncmp(cmdBuf, "digit,", 6) == 0) {
    char* p = cmdBuf + 6;
    int idx = atoi(p);
    p = strchr(p, ',');
    if (!p || *(p + 1) == '\0') {
      Serial.println("ERR:bad digit command");
      return;
    }
    char glyph = *(p + 1);
    if (idx < 0 || idx >= TOTAL_DIGITS) {
      Serial.println("ERR:digit out of range (0-18)");
      return;
    }
    setDigitGlyph(idx, glyph);
    Serial.print("OK:digit=");
    Serial.print(idx);
    Serial.print(",glyph=");
    Serial.println(glyph);
    return;
  }

  if (strncmp(cmdBuf, "raw,", 4) == 0) {
    char* p = cmdBuf + 4;
    int idx = atoi(p);
    p = strchr(p, ',');
    if (!p || *(p + 1) == '\0') {
      Serial.println("ERR:bad raw command");
      return;
    }
    int rawValue = atoi(p + 1);
    if (idx < 0 || idx >= TOTAL_DIGITS || rawValue < 0 || rawValue > 255) {
      Serial.println("ERR:raw args out of range");
      return;
    }
    setDigitRaw(idx, (byte)rawValue);
    Serial.print("OK:raw digit=");
    Serial.print(idx);
    Serial.print(" value=");
    Serial.println(rawValue);
    return;
  }

  Serial.println("ERR:unknown command");
  printHelp();
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  pinMode(SRCK1, OUTPUT); pinMode(SERIN1, OUTPUT); pinMode(RCK1, OUTPUT);
  pinMode(SRCK2, OUTPUT); pinMode(SERIN2, OUTPUT); pinMode(RCK2, OUTPUT);
  digitalWrite(SRCK1, LOW); digitalWrite(SERIN1, LOW); digitalWrite(RCK1, LOW);
  digitalWrite(SRCK2, LOW); digitalWrite(SERIN2, LOW); digitalWrite(RCK2, LOW);

  for (int i = 0; i < TOTAL_DIGITS; i++) displayBuf[i] = 0;
  safeDelay(1000);

  // Show 8 on first digit of each chain to verify wiring on boot
  Serial.println("Boot: showing 8 on digit 0 (chain1) and digit 9 (chain2)...");
  displayBuf[0] = SEGS[8];
  displayBuf[NUM_DIGITS1] = SEGS[8];
  refreshDisplay();

  printHelp();
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '#' || c == '\n' || c == '\r') {
      if (!discardingCmd && cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        processCommand();
      }
      discardingCmd = false;
      cmdLen = 0;
    } else if (cmdLen < sizeof(cmdBuf) - 1) {
      if (!discardingCmd) {
        cmdBuf[cmdLen++] = c;
      }
    } else {
      Serial.println("ERR:cmd too long");
      discardingCmd = true;
      cmdLen = 0;
    }
  }
}
