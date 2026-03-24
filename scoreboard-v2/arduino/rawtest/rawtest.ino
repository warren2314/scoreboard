// Hardware SPI test v2 - slower clock for reliability
// SRCK=13(SCK), SERIN=11(MOSI), RCK=10(SS/latch)

#include <SPI.h>

#define RCK 10

const byte SEGS[] = {252, 96, 218, 242, 102, 182, 190, 224, 254, 230};

void latch() {
  delayMicroseconds(50);
  digitalWrite(RCK, HIGH);
  delayMicroseconds(50);
  digitalWrite(RCK, LOW);
  delayMicroseconds(50);
}

void displayStr(const char* s, int n) {
  digitalWrite(RCK, LOW);
  delayMicroseconds(50);
  for (int x = n - 1; x >= 0; x--) {
    byte val = 0;
    if (s[x] >= '0' && s[x] <= '9') val = SEGS[s[x] - '0'];
    SPI.transfer(val);
    delayMicroseconds(50);
  }
  latch();
}

void displayAll(byte val, int n) {
  digitalWrite(RCK, LOW);
  delayMicroseconds(50);
  for (int i = 0; i < n; i++) {
    SPI.transfer(val);
    delayMicroseconds(50);
  }
  latch();
}

void setup() {
  Serial.begin(57600);
  pinMode(RCK, OUTPUT);
  digitalWrite(RCK, LOW);
  SPI.begin();
  SPI.beginTransaction(SPISettings(1000000, LSBFIRST, SPI_MODE0));
  delay(500);
  Serial.println("SPI TEST READY");
}

void loop() {
  Serial.println("Phase 1: ALL OFF");
  displayAll(0, 9);
  delay(5000);

  Serial.println("Phase 2: ALL 8s (254)");
  displayAll(254, 9);
  delay(5000);

  Serial.println("Phase 3: ALL 1s (96)");
  displayAll(96, 9);
  delay(5000);

  Serial.println("Phase 4: ALL 0s (252)");
  displayAll(252, 9);
  delay(5000);
}
