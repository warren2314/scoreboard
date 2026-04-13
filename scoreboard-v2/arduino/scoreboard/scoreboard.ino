// Droylsden Cricket Club - Production Scoreboard Sketch
//
// 3 chains, 6 digits each, 18 digits total.
// All chains use confirmed pin order: SRCK, SERIN, RCK
//
// Chain 1 (pins 2,3,4) — 6 digits:
//   BatB[0] BatB[1] BatB[2] | Wkts[0] | Overs[0] Overs[1]
//
// Chain 2 (pins 5,6,7) — 6 digits:
//   Total[0] Total[1] Total[2] | BatA[0] BatA[1] BatA[2]
//
// Chain 3 (pins 8,9,10) — 6 digits:
//   Target[0] Target[1] Target[2] | DLS[0] DLS[1] DLS[2]
//
// Command format (57600 baud, # terminated):
//   4,<batA(3)>,<total(3)>,<batB(3)>,<target(3)>,<wkts(1)>,<overs(2)>,<dls(3)>#
//   Example: 4,--5,123,--8,--0,3,12,--0#
//
// Other commands:
//   5#        test mode (all 8s)
//   clear#    all digits off
//   status#   print current values

#include <stdlib.h>
#include <string.h>

// Chain 1
#define SRCK1  2
#define SERIN1 3
#define RCK1   4

// Chain 2
#define SRCK2  5
#define SERIN2 6
#define RCK2   7

// Chain 3
#define SRCK3  8
#define SERIN3 9
#define RCK3   10

#define DIGITS_PER_CHAIN 6
#define SERIAL_BAUD 57600

// 7-segment encoding: segments a-g, LSB first
// Confirmed working on this hardware
const byte SEGS[] = {252, 96, 218, 242, 102, 182, 190, 224, 254, 230};

// Display buffers — one per chain
byte chain1[DIGITS_PER_CHAIN];  // BatA(3) + Wkts(1) + Overs(2)
byte chain2[DIGITS_PER_CHAIN];  // Total(3) + BatB(3)
byte chain3[DIGITS_PER_CHAIN];  // Target(3) + DLS(3)

// Serial command buffer
char cmdBuf[64];
uint8_t cmdLen = 0;
bool discardingCmd = false;

// Current score strings (for status command)
char batA[4]   = "--0";
char total[4]  = "--0";
char batB[4]   = "--0";
char target[4] = "--0";
char wkts[2]   = "0";
char overs[3]  = "-0";
char dls[4]    = "--0";

// --- Shift register drivers ---

byte encodeGlyph(char c) {
  if (c == '-') return 0;
  if (c >= '0' && c <= '9') return SEGS[c - '0'];
  return 0;
}

void shiftByte(int srckPin, int serinPin, byte value) {
  for (byte bit = 0; bit < 8; bit++) {
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

void sendChain(int srckPin, int serinPin, int rckPin, const byte* buf) {
  digitalWrite(rckPin, LOW);
  for (int i = DIGITS_PER_CHAIN - 1; i >= 0; i--) {
    shiftByte(srckPin, serinPin, buf[i]);
  }
  latch(rckPin);
}

void refreshAll() {
  sendChain(SRCK1, SERIN1, RCK1, chain1);
  sendChain(SRCK2, SERIN2, RCK2, chain2);
  sendChain(SRCK3, SERIN3, RCK3, chain3);
}

// --- Display helpers ---

void clearAll() {
  memset(chain1, 0, sizeof(chain1));
  memset(chain2, 0, sizeof(chain2));
  memset(chain3, 0, sizeof(chain3));
  refreshAll();
}

void testMode() {
  // Show 8 on all digits
  memset(chain1, SEGS[8], sizeof(chain1));
  memset(chain2, SEGS[8], sizeof(chain2));
  memset(chain3, SEGS[8], sizeof(chain3));
  refreshAll();
}

// Map score strings into display buffers and refresh
void applyScore() {
  // Chain 1: BatB(0-2) | Wkts(3) | Overs(4-5)
  chain1[0] = encodeGlyph(batB[0]);
  chain1[1] = encodeGlyph(batB[1]);
  chain1[2] = encodeGlyph(batB[2]);
  chain1[3] = encodeGlyph(wkts[0]);
  chain1[4] = encodeGlyph(overs[0]);
  chain1[5] = encodeGlyph(overs[1]);

  // Chain 2: Total(0-2) | BatA(3-5)
  chain2[0] = encodeGlyph(total[0]);
  chain2[1] = encodeGlyph(total[1]);
  chain2[2] = encodeGlyph(total[2]);
  chain2[3] = encodeGlyph(batA[0]);
  chain2[4] = encodeGlyph(batA[1]);
  chain2[5] = encodeGlyph(batA[2]);

  // Chain 3: Target(0-2) | DLS(3-5)
  chain3[0] = encodeGlyph(target[0]);
  chain3[1] = encodeGlyph(target[1]);
  chain3[2] = encodeGlyph(target[2]);
  chain3[3] = encodeGlyph(dls[0]);
  chain3[4] = encodeGlyph(dls[1]);
  chain3[5] = encodeGlyph(dls[2]);

  refreshAll();
}

// --- Command parser ---

// Copy exactly len chars from src into dest, null terminate
// Returns false if src doesn't have enough characters
bool extractField(const char* src, char* dest, int len) {
  if ((int)strlen(src) < len) return false;
  strncpy(dest, src, len);
  dest[len] = '\0';
  return true;
}

void processCommand() {
  if (cmdLen == 0) return;

  // Production score command: 4,batA,total,batB,target,wkts,overs,dls#
  if (cmdBuf[0] == '4' && cmdBuf[1] == ',') {
    char* p = cmdBuf + 2;
    char* fields[8];
    int fieldCount = 0;
    fields[fieldCount++] = p;
    while (*p && fieldCount < 8) {
      if (*p == ',') {
        *p = '\0';
        fields[fieldCount++] = p + 1;
      }
      p++;
    }

    if (fieldCount < 7) {
      Serial.println("ERR:bad score command, expected 4,batA(3),total(3),batB(3),target(3),wkts(1),overs(2),dls(3)");
      return;
    }

    if (strlen(fields[0]) != 3 || strlen(fields[1]) != 3 || strlen(fields[2]) != 3 ||
        strlen(fields[3]) != 3 || strlen(fields[4]) != 1 || strlen(fields[5]) != 2 ||
        (fieldCount >= 8 && strlen(fields[6]) != 3) ) {
      Serial.println("ERR:field length mismatch");
      return;
    }

    strncpy(batA,   fields[0], 4);
    strncpy(total,  fields[1], 4);
    strncpy(batB,   fields[2], 4);
    strncpy(target, fields[3], 4);
    strncpy(wkts,   fields[4], 2);
    strncpy(overs,  fields[5], 3);
    if (fieldCount >= 8) strncpy(dls, fields[6], 4);

    applyScore();
    Serial.print("OK:score batA="); Serial.print(batA);
    Serial.print(" total="); Serial.print(total);
    Serial.print(" batB="); Serial.print(batB);
    Serial.print(" target="); Serial.print(target);
    Serial.print(" wkts="); Serial.print(wkts);
    Serial.print(" overs="); Serial.print(overs);
    Serial.print(" dls="); Serial.println(dls);
    return;
  }

  // Test mode: 5#
  if (cmdBuf[0] == '5' && cmdLen == 1) {
    testMode();
    Serial.println("OK:test mode - all 8s");
    return;
  }

  // Clear
  if (strcmp(cmdBuf, "clear") == 0) {
    clearAll();
    Serial.println("OK:clear");
    return;
  }

  // Status
  if (strcmp(cmdBuf, "status") == 0) {
    Serial.print("batA="); Serial.print(batA);
    Serial.print(" total="); Serial.print(total);
    Serial.print(" batB="); Serial.print(batB);
    Serial.print(" target="); Serial.print(target);
    Serial.print(" wkts="); Serial.print(wkts);
    Serial.print(" overs="); Serial.print(overs);
    Serial.print(" dls="); Serial.println(dls);
    return;
  }

  Serial.print("ERR:unknown cmd ");
  Serial.println(cmdBuf);
}

void setup() {
  Serial.begin(SERIAL_BAUD);

  // Initialise all chain pins
  int pins[] = {SRCK1, SERIN1, RCK1, SRCK2, SERIN2, RCK2, SRCK3, SERIN3, RCK3};
  for (int i = 0; i < 9; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }

  delay(500);
  clearAll();

  Serial.println("Droylsden CC Scoreboard ready");
  Serial.println("Command: 4,batA(3),total(3),batB(3),target(3),wkts(1),overs(2),dls(3)#");
  Serial.println("Other: 5# (test), clear#, status#");
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
