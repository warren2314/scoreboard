// Single-digit test for one Shift2A chain.
//
// Default target is the currently working set-1 wiring discovered on 2026-03-20:
//   SRCK  -> D4
//   SERIN -> D3
//   RCK   -> D2
//   DIGITS -> 6
//
// IMPORTANT:
// - Digit index 0 is the first byte in the logical buffer, not necessarily the
//   left-most physical digit. Run the "walk#" command once to learn the order.
// - Commands are newline or '#' terminated so they work in Serial Monitor.
//
// Commands:
//   help#
//   walk#
//   clear#
//   delay,20#
//   digit,3,8#
//   digit,1,-#
//   raw,2,254#
//   scan,4#

#include <stdlib.h>
#include <string.h>

#define SRCK 4
#define SERIN 3
#define RCK 2
#define NUM_DIGITS 6
#define SERIAL_BAUD 57600

const byte SEGS[] = {252, 96, 218, 242, 102, 182, 190, 224, 254, 230};
unsigned int pulseDelayMs = 10;  // milliseconds per shift step

char cmdBuf[48];
uint8_t cmdLen = 0;
bool discardingCmd = false;

byte encodeGlyph(char glyph) {
  if (glyph == '-') return 0;
  if (glyph >= '0' && glyph <= '9') return SEGS[glyph - '0'];
  return 0;
}

void shiftByte(byte value) {
  for (byte bit = 0; bit < 8; bit++) {
    digitalWrite(SRCK, LOW);
    delay(pulseDelayMs);
    digitalWrite(SERIN, (value & 0x01) ? HIGH : LOW);
    delay(pulseDelayMs);
    digitalWrite(SRCK, HIGH);
    delay(pulseDelayMs);
    value >>= 1;
  }

  digitalWrite(SRCK, LOW);
  delay(pulseDelayMs);
  digitalWrite(SERIN, LOW);
}

void latch() {
  delay(pulseDelayMs);
  digitalWrite(RCK, HIGH);
  delay(pulseDelayMs);
  digitalWrite(RCK, LOW);
  delay(pulseDelayMs);
}

void displayRawDigits(const byte* digits) {
  digitalWrite(RCK, LOW);
  delay(pulseDelayMs);
  for (int i = NUM_DIGITS - 1; i >= 0; i--) {
    shiftByte(digits[i]);
  }
  latch();
}

void clearDisplay() {
  byte digits[NUM_DIGITS];
  for (int i = 0; i < NUM_DIGITS; i++) {
    digits[i] = 0;
  }
  displayRawDigits(digits);
}

void showOneDigitRaw(int digitIndex, byte rawValue) {
  byte digits[NUM_DIGITS];
  for (int i = 0; i < NUM_DIGITS; i++) {
    digits[i] = 0;
  }

  if (digitIndex >= 0 && digitIndex < NUM_DIGITS) {
    digits[digitIndex] = rawValue;
  }

  displayRawDigits(digits);
}

void showOneDigitGlyph(int digitIndex, char glyph) {
  showOneDigitRaw(digitIndex, encodeGlyph(glyph));
}

// Non-blocking wait that keeps USB alive
void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) {
    yield();
  }
}

void walkDigits() {
  Serial.println("Walking an 8 across every logical digit index...");
  for (int i = 0; i < NUM_DIGITS; i++) {
    Serial.print("digit=");
    Serial.println(i);
    showOneDigitGlyph(i, '8');
    safeDelay(2000);
  }
  clearDisplay();
}

void scanDigit(int digitIndex) {
  if (digitIndex < 0 || digitIndex >= NUM_DIGITS) {
    Serial.println("ERR:digit out of range");
    return;
  }

  Serial.print("Scanning digit ");
  Serial.println(digitIndex);
  for (char glyph = '0'; glyph <= '9'; glyph++) {
    Serial.print("glyph=");
    Serial.println(glyph);
    showOneDigitGlyph(digitIndex, glyph);
    safeDelay(2000);
  }
  clearDisplay();
}

void printHelp() {
  Serial.println("single_digit_test ready");
  Serial.print("Pins: SRCK=");
  Serial.print(SRCK);
  Serial.print(" SERIN=");
  Serial.print(SERIN);
  Serial.print(" RCK=");
  Serial.print(RCK);
  Serial.print(" digits=");
  Serial.println(NUM_DIGITS);
  Serial.print("pulseDelayMs=");
  Serial.println(pulseDelayMs);
  Serial.println("Commands: help#, walk#, clear#, delay,<ms>#");
  Serial.println("          digit,<index>,<0-9 or ->#");
  Serial.println("          raw,<index>,<0-255>#");
  Serial.println("          scan,<index>#");
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
    int digitIndex = atoi(cmdBuf + 5);
    scanDigit(digitIndex);
    return;
  }

  if (strncmp(cmdBuf, "digit,", 6) == 0) {
    char* p = cmdBuf + 6;
    int digitIndex = atoi(p);
    p = strchr(p, ',');
    if (!p || *(p + 1) == '\0') {
      Serial.println("ERR:bad digit command");
      return;
    }
    char glyph = *(p + 1);
    if (digitIndex < 0 || digitIndex >= NUM_DIGITS) {
      Serial.println("ERR:digit out of range");
      return;
    }
    showOneDigitGlyph(digitIndex, glyph);
    Serial.print("OK:digit=");
    Serial.print(digitIndex);
    Serial.print(",glyph=");
    Serial.println(glyph);
    return;
  }

  if (strncmp(cmdBuf, "raw,", 4) == 0) {
    char* p = cmdBuf + 4;
    int digitIndex = atoi(p);
    p = strchr(p, ',');
    if (!p || *(p + 1) == '\0') {
      Serial.println("ERR:bad raw command");
      return;
    }
    int rawValue = atoi(p + 1);
    if (digitIndex < 0 || digitIndex >= NUM_DIGITS || rawValue < 0 || rawValue > 255) {
      Serial.println("ERR:raw args out of range");
      return;
    }
    showOneDigitRaw(digitIndex, (byte)rawValue);
    Serial.print("OK:raw digit=");
    Serial.print(digitIndex);
    Serial.print(" value=");
    Serial.println(rawValue);
    return;
  }

  Serial.println("ERR:unknown command");
  printHelp();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(SRCK, OUTPUT);
  pinMode(SERIN, OUTPUT);
  pinMode(RCK, OUTPUT);
  digitalWrite(SRCK, LOW);
  digitalWrite(SERIN, LOW);
  digitalWrite(RCK, LOW);
  safeDelay(1000);
  clearDisplay();
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
