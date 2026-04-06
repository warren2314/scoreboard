// Speed test for Shift2A / TPIC6B595 boards using the corrected pin mapping.
//   SRCK  -> D4
//   SERIN -> D3
//   RCK   -> D2

#define SRCK 2
#define SERIN 3
#define RCK 4
#define NUM_DIGITS 6
#define SERIAL_BAUD 57600

const byte SEGS[] = {252, 96, 218, 242, 102, 182, 190, 224, 254, 230};
const unsigned int PULSE_DELAY_MS = 50;

void shiftByte(byte value) {
  for (byte bit = 0; bit < 8; bit++) {
    digitalWrite(SRCK, LOW);
    delay(PULSE_DELAY_MS);
    digitalWrite(SERIN, (value & 0x01) ? HIGH : LOW);
    delay(PULSE_DELAY_MS);
    digitalWrite(SRCK, HIGH);
    delay(PULSE_DELAY_MS);
    value >>= 1;
  }
  digitalWrite(SRCK, LOW);
  delay(PULSE_DELAY_MS);
  digitalWrite(SERIN, LOW);
}

void latch() {
  delay(PULSE_DELAY_MS);
  digitalWrite(RCK, HIGH);
  delay(PULSE_DELAY_MS);
  digitalWrite(RCK, LOW);
  delay(PULSE_DELAY_MS);
}

void displayAll(byte value) {
  digitalWrite(RCK, LOW);
  delay(PULSE_DELAY_MS);
  for (int i = 0; i < NUM_DIGITS; i++) {
    shiftByte(value);
  }
  latch();
}

void displayStr(const char* text) {
  digitalWrite(RCK, LOW);
  delay(PULSE_DELAY_MS);
  for (int i = NUM_DIGITS - 1; i >= 0; i--) {
    byte value = 0;
    if (text[i] >= '0' && text[i] <= '9') {
      value = SEGS[text[i] - '0'];
    }
    shiftByte(value);
  }
  latch();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(SRCK, OUTPUT);
  pinMode(SERIN, OUTPUT);
  pinMode(RCK, OUTPUT);
  digitalWrite(SRCK, LOW);
  digitalWrite(SERIN, LOW);
  digitalWrite(RCK, LOW);
  delay(2000);
  Serial.print("slowtest pulseDelayMs=");
  Serial.println(PULSE_DELAY_MS);
}

void loop() {
  Serial.println("Phase 1: ALL OFF");
  displayAll(0);
  delay(10000);

  Serial.println("Phase 2: ALL 8s");
  displayAll(254);
  delay(10000);

  Serial.println("Phase 3: 123456");
  displayStr("123456");
  delay(10000);

  Serial.println("Phase 4: ALL 0s");
  displayStr("000000");
  delay(10000);
}
