// Segment Wiring Diagnostic
// Lights up one shift register output at a time on the corrected pin mapping.
// Watch which physical segment lights up and note it down.
//
// Step 1 = Q0 lit, Step 2 = Q1 lit, ... Step 8 = Q7 lit
// The LED on pin 13 blinks the step number (1 blink = step 1, etc.)

#define SRCK1 2
#define SERIN1 3
#define RCK1 4
#define SRCK2 7
#define SERIN2 6
#define RCK2 5
#define LED 13

#define NUM_DIGITS1 6
#define NUM_DIGITS2 6
#define STEP_DELAY 30000
const unsigned int PULSE_US = 50;

void shiftByteSlow(int serin, int srck, byte val) {
  for (int bit = 0; bit < 8; bit++) {
    digitalWrite(srck, LOW);
    digitalWrite(serin, (val & 0x01) ? HIGH : LOW);
    delayMicroseconds(PULSE_US);
    digitalWrite(srck, HIGH);
    delayMicroseconds(PULSE_US);
    val >>= 1;
  }

  digitalWrite(srck, LOW);
  digitalWrite(serin, LOW);
}

void shiftByte(int serin, int srck, int rck, int numDigits, byte val) {
  digitalWrite(rck, LOW);
  for (int i = 0; i < numDigits; i++) {
    shiftByteSlow(serin, srck, val);
  }
  digitalWrite(rck, HIGH);
  delayMicroseconds(PULSE_US);
  digitalWrite(rck, LOW);
}

void blinkStep(int step) {
  for (int i = 0; i < step; i++) {
    digitalWrite(LED, HIGH); delay(150);
    digitalWrite(LED, LOW);  delay(150);
  }
}

void setup() {
  Serial.begin(57600);
  pinMode(SRCK1, OUTPUT); pinMode(SERIN1, OUTPUT); pinMode(RCK1, OUTPUT);
  pinMode(SRCK2, OUTPUT); pinMode(SERIN2, OUTPUT); pinMode(RCK2, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(SRCK1, LOW); digitalWrite(SERIN1, LOW); digitalWrite(RCK1, LOW);
  digitalWrite(SRCK2, LOW); digitalWrite(SERIN2, LOW); digitalWrite(RCK2, LOW);
  Serial.println("Diagnostic starting...");
}

void loop() {
  for (int bit = 0; bit < 8; bit++) {
    byte val = (1 << bit);
    shiftByte(SERIN1, SRCK1, RCK1, NUM_DIGITS1, val);
    shiftByte(SERIN2, SRCK2, RCK2, NUM_DIGITS2, val);
    Serial.print("Step "); Serial.print(bit + 1);
    Serial.print(" - Q"); Serial.print(bit);
    Serial.print(" ON (value="); Serial.print(val); Serial.println(")");
    blinkStep(bit + 1);
    delay(STEP_DELAY);
  }
  // All off
  shiftByte(SERIN1, SRCK1, RCK1, NUM_DIGITS1, 0);
  shiftByte(SERIN2, SRCK2, RCK2, NUM_DIGITS2, 0);
  Serial.println("All off - restarting cycle...");
  delay(1000);
}
