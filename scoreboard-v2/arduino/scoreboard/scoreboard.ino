// Cricket Scoreboard - Raw Pin Toggle Test
// Droylsden Cricket Club
// Tests individual pins to identify wiring
//
// On boot: sets pins 2,3,4 as OUTPUT and LOW
// Send these commands (57600 baud, # terminated):
//   h2# = pin 2 HIGH     l2# = pin 2 LOW
//   h3# = pin 3 HIGH     l3# = pin 3 LOW
//   h4# = pin 4 HIGH     l4# = pin 4 LOW
//   c#  = all pins LOW (clear)
//   t#  = toggle test: pulses each pin for 1 second

#define LED_PIN LED_BUILTIN
#define SERIAL_BAUD 57600
#define CMD_BUF_SIZE 16

char cmdBuf[CMD_BUF_SIZE];
uint8_t cmdLen = 0;

unsigned long lastBlink = 0;
bool ledState = false;

void processCommand() {
  if (cmdLen == 0) return;

  if (cmdBuf[0] == 'h' || cmdBuf[0] == 'l') {
    int pin = atoi(&cmdBuf[1]);
    if (pin >= 2 && pin <= 7) {
      bool state = (cmdBuf[0] == 'h');
      digitalWrite(pin, state ? HIGH : LOW);
      Serial.print("OK:pin ");
      Serial.print(pin);
      Serial.println(state ? " HIGH" : " LOW");
    } else {
      Serial.println("ERR:pin must be 2-7");
    }
    return;
  }

  if (cmdBuf[0] == 'c') {
    for (int p = 2; p <= 7; p++) {
      digitalWrite(p, LOW);
    }
    Serial.println("OK:all pins LOW");
    return;
  }

  if (cmdBuf[0] == 't') {
    Serial.println("OK:toggle test starting");
    for (int p = 2; p <= 4; p++) {
      Serial.print("Toggling pin ");
      Serial.println(p);
      digitalWrite(p, HIGH);
      delay(1000);
      digitalWrite(p, LOW);
      delay(500);
    }
    Serial.println("OK:toggle test done");
    return;
  }

  Serial.print("ERR:unknown cmd ");
  Serial.println(cmdBuf);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  pinMode(LED_PIN, OUTPUT);

  for (int p = 2; p <= 7; p++) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }

  Serial.println("RAW PIN TEST READY");
  Serial.println("h2# h3# h4# = set pin HIGH");
  Serial.println("l2# l3# l4# = set pin LOW");
  Serial.println("c# = all LOW, t# = toggle test");
}

void loop() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '#' || c == '\n') {
      if (cmdLen > 0) {
        cmdBuf[cmdLen] = '\0';
        processCommand();
      }
      cmdLen = 0;
    } else if (c != '\r') {
      if (cmdLen < CMD_BUF_SIZE - 1) {
        cmdBuf[cmdLen++] = c;
      }
    }
  }

  unsigned long now = millis();
  if (now - lastBlink >= 2000) {
    lastBlink = now;
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }
}
