// Droylsden Cricket Club — Production Scoreboard Sketch (with built-in diagnostics)
//
// Hardware: 3 chains of TPIC6B595 shift registers, 6 digits per chain = 18 total
//
//   Chain 1 (SRCK=D2, SERIN=D3, RCK=D4) — 6 digits
//     Index 0-2: BatA     Index 3-5: Total
//
//   Chain 2 (SRCK=D5, SERIN=D6, RCK=D7) — 6 digits
//     Index 6-8: BatB     Index 9: Wkts     Index 10-11: Overs
//
//   Chain 3 (SRCK=D8, SERIN=D9, RCK=D10) — 6 digits
//     Index 12-14: Target  Index 15-17: DLS
//
// Production command (57600 baud, # terminated):
//   4,<batA(3)>,<total(3)>,<batB(3)>,<target(3)>,<wkts(1)>,<overs(2)>,<dls(3)>#
//   Example: 4,--5,123,--8,--0,3,12,--0#
//
// Diagnostic commands (all work from web admin serial console too):
//   5#               test mode — all digits show 8
//   alltest#         same as 5#
//   clear#           all digits off
//   walk#            walk an 8 across all 18 digits (2s each), prints current index
//   observe#         clear, then show one 0 at a time on each digit (slow)
//   observefast#     same as observe# but faster
//   status#          print last known score values
//   digit,X,Y#       set digit X (0-17) to glyph Y (0-9 or - for blank)
//   scan,X#          slowly cycle digit X through 0-9
//   raw,X,Y#         set digit X to raw segment byte Y (0-255) for wiring checks
//   help#            print this message

#include <stdlib.h>
#include <string.h>

// Chain 1 — BatA, Total
#define SRCK1  2
#define SERIN1 3
#define RCK1   4

// Chain 2 — BatB, Wkts, Overs
#define SRCK2  5
#define SERIN2 6
#define RCK2   7

// Chain 3 — Target, DLS
#define SRCK3  8
#define SERIN3 9
#define RCK3   10

#define DIGITS_PER_CHAIN 6
#define TOTAL_DIGITS     18   // 3 x 6
#define SERIAL_BAUD      57600
#define OBSERVE_DELAY_MS      1500
#define OBSERVE_FAST_DELAY_MS 300

// 7-segment encoding, LSB-first, confirmed working on this hardware
//                      0    1    2    3    4    5    6    7    8    9
const byte SEGS[] = {252, 96, 218, 242, 102, 182, 190, 224, 254, 230};

// Flat display buffer — indices 0-5 = chain1, 6-11 = chain2, 12-17 = chain3
byte displayBuf[TOTAL_DIGITS];

// Last-known score strings (for status command)
char batA[4]   = "--0";
char total[4]  = "--0";
char batB[4]   = "--0";
char target[4] = "--0";
char wkts[2]   = "0";
char overs[3]  = "-0";
char dls[4]    = "--0";

// Digits that physically have a segment connection (0 = no segments driven at boot)
// These are set to '0' during staged startup to settle floating lines
const int STARTUP_ZERO_DIGITS[] = {2, 5, 8, 9, 11, 14, 17};
const int STARTUP_ZERO_COUNT    = 7;

// Serial command buffer
char    cmdBuf[64];
uint8_t cmdLen = 0;
bool    discardingCmd = false;

// ---------------------------------------------------------------------------
// Low-level shift register drivers
// ---------------------------------------------------------------------------

byte encodeGlyph(char c) {
  if (c == '-') return 0;
  if (c >= '0' && c <= '9') return SEGS[c - '0'];
  return 0;
}

void shiftByte(int srckPin, int serinPin, byte value) {
  for (byte b = 0; b < 8; b++) {
    digitalWrite(srckPin, LOW);
    digitalWrite(serinPin, (value & 0x01) ? HIGH : LOW);
    delayMicroseconds(100);
    digitalWrite(srckPin, HIGH);
    delayMicroseconds(100);
    value >>= 1;
  }
  digitalWrite(srckPin, LOW);
  digitalWrite(serinPin, LOW);
}

void latch(int rckPin) {
  delayMicroseconds(100);
  digitalWrite(rckPin, HIGH);
  delayMicroseconds(100);
  digitalWrite(rckPin, LOW);
}

// Shift one chain's worth of bytes from the flat buffer and latch.
// Shifts in reverse (last index first) so index 'start' ends up at the
// last physical register. Run walk# to confirm actual physical order.
void sendChain(int srckPin, int serinPin, int rckPin, int start) {
  digitalWrite(rckPin, LOW);
  for (int i = start + DIGITS_PER_CHAIN - 1; i >= start; i--) {
    shiftByte(srckPin, serinPin, displayBuf[i]);
  }
  latch(rckPin);
}

void refreshAll() {
  sendChain(SRCK1, SERIN1, RCK1, 0);
  sendChain(SRCK2, SERIN2, RCK2, 6);
  sendChain(SRCK3, SERIN3, RCK3, 12);
}

// ---------------------------------------------------------------------------
// Display operations
// ---------------------------------------------------------------------------

void clearAll() {
  memset(displayBuf, 0, sizeof(displayBuf));
  refreshAll();
}

void allTest() {
  memset(displayBuf, SEGS[8], sizeof(displayBuf));
  refreshAll();
  Serial.println("OK:alltest - all 18 digits show 8");
}

void setDigitRaw(int idx, byte raw) {
  if (idx >= 0 && idx < TOTAL_DIGITS) {
    displayBuf[idx] = raw;
    refreshAll();
  }
}

void setDigitGlyph(int idx, char g) {
  setDigitRaw(idx, encodeGlyph(g));
}

// Map score strings into display buffer and refresh
void applyScore() {
  // Chain 1 — indices 0-5: BatA(0-2), Total(3-5)
  displayBuf[0] = encodeGlyph(batA[0]);
  displayBuf[1] = encodeGlyph(batA[1]);
  displayBuf[2] = encodeGlyph(batA[2]);
  displayBuf[3] = encodeGlyph(total[0]);
  displayBuf[4] = encodeGlyph(total[1]);
  displayBuf[5] = encodeGlyph(total[2]);

  // Chain 2 — indices 6-11: BatB(6-8), Wkts(9), Overs(10-11)
  displayBuf[6]  = encodeGlyph(batB[0]);
  displayBuf[7]  = encodeGlyph(batB[1]);
  displayBuf[8]  = encodeGlyph(batB[2]);
  displayBuf[9]  = encodeGlyph(wkts[0]);
  displayBuf[10] = encodeGlyph(overs[0]);
  displayBuf[11] = encodeGlyph(overs[1]);

  // Chain 3 — indices 12-17: Target(12-14), DLS(15-17)
  displayBuf[12] = encodeGlyph(target[0]);
  displayBuf[13] = encodeGlyph(target[1]);
  displayBuf[14] = encodeGlyph(target[2]);
  displayBuf[15] = encodeGlyph(dls[0]);
  displayBuf[16] = encodeGlyph(dls[1]);
  displayBuf[17] = encodeGlyph(dls[2]);

  refreshAll();
}

// ---------------------------------------------------------------------------
// Diagnostic helpers
// ---------------------------------------------------------------------------

void safeDelay(unsigned long ms) {
  unsigned long start = millis();
  while (millis() - start < ms) yield();
}

void zeroWalkDigits() {
  Serial.println("Walking 0 across all 18 digits...");
  for (int i = 0; i < TOTAL_DIGITS; i++) {
    memset(displayBuf, 0, sizeof(displayBuf));
    displayBuf[i] = SEGS[0];
    refreshAll();
    Serial.print("digit="); Serial.println(i);
    safeDelay(2000);
  }
  memset(displayBuf, 0, sizeof(displayBuf));
  refreshAll();
  Serial.println("OK:zerowalk done");
}

void runStagedStartup() {
  Serial.println("startup: clear all");
  clearAll();
  safeDelay(500);
  for (int i = 0; i < STARTUP_ZERO_COUNT; i++) {
    setDigitGlyph(STARTUP_ZERO_DIGITS[i], '0');
    safeDelay(150);
  }
  Serial.println("startup: complete");
}

void walkDigits() {
  Serial.println("Walking 8 across all 18 digits...");
  for (int i = 0; i < TOTAL_DIGITS; i++) {
    memset(displayBuf, 0, sizeof(displayBuf));
    displayBuf[i] = SEGS[8];
    refreshAll();
    Serial.print("digit="); Serial.println(i);
    safeDelay(2000);
  }
  memset(displayBuf, 0, sizeof(displayBuf));
  refreshAll();
  Serial.println("OK:walk done");
}

void observeDigits(unsigned long delayMs) {
  clearAll();
  Serial.println("observe: clear");
  safeDelay(delayMs);

  for (int i = 0; i < TOTAL_DIGITS; i++) {
    clearAll();
    setDigitGlyph(i, '0');
    Serial.print("observe: index ");
    Serial.print(i);
    Serial.println(" -> 0");
    safeDelay(delayMs);
  }

  clearAll();
  Serial.println("observe: complete");
}

void scanDigit(int idx) {
  if (idx < 0 || idx >= TOTAL_DIGITS) {
    Serial.println("ERR:index out of range (0-17)");
    return;
  }
  Serial.print("Scanning digit "); Serial.println(idx);
  for (char g = '0'; g <= '9'; g++) {
    displayBuf[idx] = encodeGlyph(g);
    refreshAll();
    Serial.print("glyph="); Serial.println(g);
    safeDelay(2000);
  }
  displayBuf[idx] = 0;
  refreshAll();
  Serial.println("OK:scan done");
}

void printStatus() {
  Serial.print("batA=");    Serial.print(batA);
  Serial.print(" total=");  Serial.print(total);
  Serial.print(" batB=");   Serial.print(batB);
  Serial.print(" target="); Serial.print(target);
  Serial.print(" wkts=");   Serial.print(wkts);
  Serial.print(" overs=");  Serial.print(overs);
  Serial.print(" dls=");    Serial.println(dls);
}

void printHelp() {
  Serial.println("=== Droylsden CC Scoreboard ready ===");
  Serial.println("Chain1 D2/3/4  idx 0-5   BatA(0-2) Total(3-5)");
  Serial.println("Chain2 D5/6/7  idx 6-11  BatB(6-8) Wkts(9) Overs(10-11)");
  Serial.println("Chain3 D8/9/10 idx 12-17 Target(12-14) DLS(15-17)");
  Serial.println("Production: 4,batA(3),total(3),batB(3),target(3),wkts(1),overs(2),dls(3)#");
  Serial.println("Diag: 5# alltest# clear# walk# zerowalk# observe# observefast# status# help#");
  Serial.println("      startup# (staged boot — sets end-digit zeros to settle lines)");
  Serial.println("      digit,<0-17>,<0-9|->   scan,<0-17>   raw,<0-17>,<0-255>");
}

// ---------------------------------------------------------------------------
// Command parser
// ---------------------------------------------------------------------------

void processCommand() {
  if (cmdLen == 0) return;

  // Production score update: 4,batA(3),total(3),batB(3),target(3),wkts(1),overs(2),dls(3)#
  if (cmdBuf[0] == '4' && cmdBuf[1] == ',') {
    // Split by comma into up to 7 fields (everything after "4,")
    char* p = cmdBuf + 2;
    char* fields[7];
    int   count = 0;
    fields[count++] = p;
    while (*p && count < 7) {
      if (*p == ',') { *p = '\0'; fields[count++] = p + 1; }
      p++;
    }

    if (count < 7) {
      Serial.println("ERR:bad score cmd — need 4,batA(3),total(3),batB(3),target(3),wkts(1),overs(2),dls(3)#");
      return;
    }
    if (strlen(fields[0]) != 3 || strlen(fields[1]) != 3 || strlen(fields[2]) != 3 ||
        strlen(fields[3]) != 3 || strlen(fields[4]) != 1 ||
        strlen(fields[5]) != 2 || strlen(fields[6]) != 3) {
      Serial.println("ERR:field length mismatch — batA/total/batB/target/dls=3 wkts=1 overs=2");
      return;
    }

    strncpy(batA,   fields[0], 4);
    strncpy(total,  fields[1], 4);
    strncpy(batB,   fields[2], 4);
    strncpy(target, fields[3], 4);
    strncpy(wkts,   fields[4], 2);
    strncpy(overs,  fields[5], 3);
    strncpy(dls,    fields[6], 4);  // Fixed: was gated on fieldCount>=8, now always applied

    applyScore();
    Serial.print("OK:score ");
    printStatus();
    return;
  }

  // Test mode
  if (strcmp(cmdBuf, "5") == 0 || strcmp(cmdBuf, "alltest") == 0) {
    allTest();
    return;
  }

  if (strcmp(cmdBuf, "clear") == 0) {
    clearAll();
    Serial.println("OK:clear");
    return;
  }

  if (strcmp(cmdBuf, "walk") == 0) {
    walkDigits();
    return;
  }

  if (strcmp(cmdBuf, "zerowalk") == 0) {
    zeroWalkDigits();
    return;
  }

  if (strcmp(cmdBuf, "startup") == 0) {
    runStagedStartup();
    return;
  }

  if (strcmp(cmdBuf, "observe") == 0) {
    observeDigits(OBSERVE_DELAY_MS);
    return;
  }

  if (strcmp(cmdBuf, "observefast") == 0) {
    observeDigits(OBSERVE_FAST_DELAY_MS);
    return;
  }

  if (strcmp(cmdBuf, "status") == 0) {
    printStatus();
    return;
  }

  if (strcmp(cmdBuf, "help") == 0) {
    printHelp();
    return;
  }

  if (strncmp(cmdBuf, "digit,", 6) == 0) {
    char* p = cmdBuf + 6;
    int   idx = atoi(p);
    p = strchr(p, ',');
    if (!p || *(p + 1) == '\0') { Serial.println("ERR:bad digit command"); return; }
    char g = *(p + 1);
    if (idx < 0 || idx >= TOTAL_DIGITS) { Serial.println("ERR:index out of range (0-17)"); return; }
    setDigitGlyph(idx, g);
    Serial.print("OK:digit="); Serial.print(idx);
    Serial.print(" glyph="); Serial.println(g);
    return;
  }

  if (strncmp(cmdBuf, "scan,", 5) == 0) {
    scanDigit(atoi(cmdBuf + 5));
    return;
  }

  if (strncmp(cmdBuf, "raw,", 4) == 0) {
    char* p = cmdBuf + 4;
    int   idx = atoi(p);
    p = strchr(p, ',');
    if (!p || *(p + 1) == '\0') { Serial.println("ERR:bad raw command"); return; }
    int val = atoi(p + 1);
    if (idx < 0 || idx >= TOTAL_DIGITS || val < 0 || val > 255) {
      Serial.println("ERR:args out of range");
      return;
    }
    setDigitRaw(idx, (byte)val);
    Serial.print("OK:raw digit="); Serial.print(idx);
    Serial.print(" byte="); Serial.println(val);
    return;
  }

  Serial.print("ERR:unknown command — "); Serial.println(cmdBuf);
  Serial.println("Send help# for usage.");
}

// ---------------------------------------------------------------------------
// Setup & loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(SERIAL_BAUD);

  const int pins[] = {SRCK1, SERIN1, RCK1, SRCK2, SERIN2, RCK2, SRCK3, SERIN3, RCK3};
  for (int i = 0; i < 9; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }

  runStagedStartup();

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
      if (!discardingCmd) cmdBuf[cmdLen++] = c;
    } else {
      Serial.println("ERR:command too long");
      discardingCmd = true;
      cmdLen = 0;
    }
  }
}
