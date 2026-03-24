# DCC Cricket Scoreboard - Debug Log

## Hardware
- **Arduino**: UNO R4 WiFi (Renesas RA4M1 ARM Cortex-M4)
- **Shift registers**: TPIC6B595N on custom "Shift2A" PCBs
- **Displays**: Common-anode 7-segment LED modules (two panels, 9 digits daisy-chained)
- **PCB connectors**: V+, GND, 5V, SRCK, RCK, S in, Sout, G en

## The Problem
The shift registers do not respond to data sent via any method. The display shows its power-on state (all 8s) and never changes, despite the Arduino code running correctly (confirmed via serial output).

## What Works
- Arduino receives and processes serial commands (confirmed via serial monitor)
- Arduino code executes all phases (confirmed: serial prints "Phase 1: ALL OFF", "Phase 2: ALL 8s" etc.)
- `digitalWrite()` bulk toggling ALL pins HIGH/LOW simultaneously causes display changes (proved shift registers and LEDs are connected and functional)
- The original v1 code worked on the **original Arduino UNO (ATmega328P AVR)** with the same shift register boards

## What Does NOT Work

### 1. Software bit-bang via `shiftOut()` (pins 2, 3, 4)
- Standard Arduino `shiftOut(dataPin, clockPin, LSBFIRST, value)` — no display change
- This is what the ShifterStr library uses internally

### 2. Manual `digitalWrite()` bit-bang (pins 2, 3, 4)
- Manually setting data pin, pulsing clock pin, then pulsing latch — no display change
- Despite `digitalWrite()` working for bulk toggling, the rapid pin-by-pin sequencing required by the shift protocol doesn't work

### 3. Direct port register manipulation (pins 2, 3, 4)
- Tried `R_PORT1->POSR` / `R_PORT1->PORR` for direct GPIO control
- Assumed pin mapping D2=P104, D3=P105, D4=P106 but this may be wrong on R4
- No display change

### 4. Hardware SPI via `SPI.transfer()` (pins 11, 13, 10)
- Wired: SERIN→pin 11 (MOSI), SRCK→pin 13 (SCK), RCK→pin 10 (SS/latch)
- **One partial success**: First test at 1MHz showed the display cycling through patterns — user saw "All dark, all 8s, then 886" on one panel. This is the ONLY time different numbers appeared.
- All subsequent attempts (same code, different speeds, different latch pins) failed — display stuck on power-on 8s
- Tried 1MHz and 100kHz SPI clock speeds
- Tried latch on pin 9 instead of pin 10 (to avoid SS conflict) — no change
- Serial monitor confirms code is executing all phases correctly

## Key Observations

1. **Power-on state is all 8s**: When the Arduino is plugged in, the displays show all 8s. This is the default state of the TPIC6B595 shift registers (undefined output register contents on power-up with G pin grounded).

2. **Display only changes on power cycle**: Every time the Arduino is unplugged and plugged back in, the display resets to 8s. The running sketch never changes what's displayed.

3. **The partial SPI success was not reproducible**: The one time it showed "886" and cycled through patterns may have been related to the power-on/reset timing rather than the SPI actually working.

4. **Loose wires were an early issue**: Original jumper wires on pins 2,3,4 had poor contact. User replaced them. Moving to SPI pins (10,11,13) had the same problem.

5. **R4 WiFi ARM quirk**: Global constructors run before GPIO hardware is initialised. `pinMode()` in constructors silently fails. This was fixed by adding `begin()` method, but the underlying shift protocol still doesn't work.

## Wiring

### Current (SPI test):
```
Arduino Pin 10 → RCK (latch) on shift register PCB
Arduino Pin 11 → SERIN (data) on shift register PCB
Arduino Pin 13 → SRCK (clock) on shift register PCB
```

### Original (v1 that worked on old UNO):
```
Set 1: Pin 2 → SRCK, Pin 3 → SERIN, Pin 4 → RCK (9 digits)
Set 2: Pin 5 → SRCK, Pin 6 → SERIN, Pin 7 → RCK (6 digits)
```

### Shift2A PCB connections:
```
V+    - LED power supply
GND   - Ground
5V    - Logic power
SRCK  - Shift register clock
RCK   - Storage register clock (latch)
S in  - Serial data in
Sout  - Serial data out (to next board in chain)
G en  - Output enable (should be grounded for outputs to be active)
```

## Things Still To Investigate

1. **G en pin**: The "G en" (output enable) pin on the Shift2A PCBs — is it actually grounded? If it's floating, outputs may not respond to latched data. The library docs say "Make sure the G Pin is grounded."

2. **SRCLR pin**: The library docs say "Make sure SRCLR is tied to 5V." If SRCLR is floating or LOW, the shift register is being held in clear and data is discarded on every clock.

3. **Pin 10 SS conflict**: The SPI library on R4 may auto-manage pin 10 (SS), conflicting with manual latch control. The one time SPI worked, it may have been the SPI library's SS toggling that accidentally provided the latch pulse.

4. **Try the original Arduino UNO**: If the old ATmega328P-based UNO is available, the v1 code worked on it. This would confirm whether the issue is R4-specific.

5. **Oscilloscope/logic analyser**: Verify that pins 10, 11, 13 (or 2, 3, 4) are actually outputting signals during shift operations. This would definitively confirm whether the problem is on the Arduino side or the shift register side.

6. **Wire quality/contact**: The R4 WiFi has a plastic case that may prevent pins from seating fully. Test with soldered connections or different jumper wires.

7. **5V logic levels**: The R4 WiFi GPIO is 3.3V (ARM). The TPIC6B595 datasheet specifies VIH (input high) minimum of 2.0V, so 3.3V should work — but verify this with the actual board.

8. **Power supply**: Are the shift register PCBs getting adequate 5V logic power? Check voltage at the PCB's 5V and GND pins.

## Current Sketch (rawtest.ino)
```cpp
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
  displayAll(0, 9);       // ALL OFF - 5 sec
  delay(5000);
  displayAll(254, 9);     // ALL 8s - 5 sec
  delay(5000);
  displayAll(96, 9);      // ALL 1s - 5 sec
  delay(5000);
  displayAll(252, 9);     // ALL 0s - 5 sec
  delay(5000);
}
```

## Segment Encoding (TPIC6B595 → 7-segment)
```
Bit:  7   6   5   4   3   2   1   0
Seg:  a   b   c   d   e   f   g   dp

0 = 252 (11111100) = a,b,c,d,e,f
1 = 96  (01100000) = b,c
2 = 218 (11011010) = a,b,d,e,g
3 = 242 (11110010) = a,b,c,d,g
4 = 102 (01100110) = b,c,f,g
5 = 182 (10110110) = a,c,d,f,g
6 = 190 (10111110) = a,c,d,e,f,g
7 = 224 (11100000) = a,b,c
8 = 254 (11111110) = a,b,c,d,e,f,g
9 = 230 (11100110) = a,b,c,f,g
```

## Latest Investigation (2026-03-20)

- The software-side ARM constructor issue is already fixed in `scoreboard-v2/arduino/scoreboard/scoreboard.ino` via `begin()` in `setup()`. That was real, but it does **not** explain the current "display never updates" state on its own.
- `ShifterStr` has now been changed to use a **manual slow bit-bang implementation** instead of Arduino `shiftOut()`. This removes `shiftOut()` timing/core behavior as a variable on the UNO R4 WiFi.
- A new sketch, `scoreboard-v2/arduino/slowtest/slowtest.ino`, now drives the production pins (`D2/D3/D4`) with long visible pulses and simple patterns. If this still does not move the display, the fault is almost certainly outside the application logic.
- Photo review suggests the next checks should be **hardware continuity/configuration**, in this order:
  1. Confirm `G en` is really strapped to the required state on the Shift2A board.
  2. Confirm the board jumper / trace that should hold `SRCLR` high is actually present.
  3. Continuity-check Arduino `D2 -> SRCK`, `D3 -> SERIN`, `D4 -> RCK` end-to-end at the Shift2A input pads, not just by wire colour.
  4. If possible, scope or LED-probe the three lines while `slowtest.ino` is running.
- The fact that **power-cycling changes the display but the running sketch does not** is more consistent with a control-pin / wiring / board-state problem than with a score-formatting bug in the Arduino sketch.

## Breakthrough Session (2026-03-20 evening)

### SRCK and RCK pins are SWAPPED

The original v1 code assumed `SRCK=D2, SERIN=D3, RCK=D4`. Through systematic testing, we discovered that **SRCK and RCK are swapped** in the physical wiring:

- **Correct mapping**: `SRCK=D4, SERIN=D3, RCK=D2`
- **Also for set 2**: `SRCK=D7, SERIN=D6, RCK=D5`

Evidence:
1. With original pin assignment at 200ms delays, the display went through "crazy things" — intermediate states being latched on every bit clock, consistent with RCK being pulsed for every bit instead of SRCK.
2. Swapping SRCK↔RCK in software produced the first recognizable digit: a **7**.
3. Sending score commands showed recognizable (though imperfect) numbers like "A98".

### Minimum timing: 200ms per step works, faster does not (yet)

- **200ms delay per step**: Confirmed working — display went fully dark (all zeros) and back to all 8s (all 0xFE). First time the display changed from its power-on state via shift protocol.
- **50ms delay per step**: Did NOT work — display stayed on power-on 8s.
- **10ms delay per step**: Partially worked — showed recognizable digits (86, 76, 0, 2) but with corruption and slow segment transitions.
- **1ms and microsecond delays**: No effect on display.

This extremely slow minimum timing (200ms = 5 seconds per byte, ~30 seconds for 6 digits) is abnormal for TPIC6B595 chips rated for 25MHz. Possible causes:
- Long wire runs causing capacitance
- 3.3V GPIO drive from R4 vs 5V from original AVR UNO
- Weak pull-up/pull-down on the Shift2A PCB traces
- `digitalWrite()` on R4 may have unusual output impedance characteristics

### Digit count mismatch

The firmware was configured for **9 digits** on set 1, but only **6 digits** (2 boards of 3) are physically connected. Shifting 9 bytes into a 6-register chain causes the first 3 bytes to overflow, corrupting the displayed data. Fixed to 6 digits for testing.

### Near-success: 888888 command

Sending `4,888,888,...` with 6-digit config and 10ms delays showed **988888** — 5 of 6 digits correct. The first digit had a consistent bit error (8→9, differing by 2 bits). Suggests timing is marginal at 10ms.

### Current state

- **Pin mapping**: SRCK=D4, SERIN=D3, RCK=D2 (swapped from original)
- **Delay**: 10ms per step in ShifterStr library
- **Digit count**: Set 1 = 6 (for testing with 2 boards)
- **Status**: Data reaches display but with bit errors at 10ms. Untested at intermediate delays (20ms-100ms). Need to find the sweet spot between speed and reliability.

### Still to do

1. Binary search the minimum reliable delay between 10ms (corrupted) and 200ms (working)
2. Confirm test mode (111222) displays correctly
3. Test end-to-end via the web admin panel
4. Wire up set 2 (pins 5,6,7 → corrected to SRCK=7, SERIN=6, RCK=5)
5. Investigate why timing needs to be so slow — may be a hardware issue worth fixing (pull-up resistors, decoupling caps, wire length)
6. Restore to full 9-digit (3 board) configuration once third board is connected

## Deeper Read (2026-03-21)

- The `SRCK`/`RCK` swap is the most important finding so far because it invalidates many of the earlier "software method X does not work" conclusions. Several of those tests were effectively clocking the latch instead of the shift register.
- The current timing conclusion is still **not cleanly isolated** because multiple variables changed across the same session:
  - pin mapping was corrected
  - digit count changed from 9 to 6
  - pulse delays changed between runs
  - different sketches were used
- Because of that, "10ms corrupted / 50ms failed / 200ms worked" should be treated as a **working observation**, not yet a proven electrical limit of the TPIC6B595 chain.
- A new sketch, `scoreboard-v2/arduino/single_digit_test/single_digit_test.ino`, has been added specifically to isolate one logical digit at a time on the corrected pins. This should be the next clean test.
- Recommended next experiment:
  1. Keep the corrected pins fixed (`SRCK=4`, `SERIN=3`, `RCK=2`)
  2. Keep digit count fixed at 6
  3. Use `walk#` to map logical digit index to physical position
  4. Use `scan,<index>#` on one known digit only
  5. Change only `delay,<ms>#` between runs
- If a **single digit** still needs very large millisecond delays to behave reliably, the remaining suspects are much more likely to be wiring/contact/signal-integrity or board-level configuration than application logic.
