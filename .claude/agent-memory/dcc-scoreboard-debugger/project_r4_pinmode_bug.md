---
name: R4 WiFi pinMode global constructor bug
description: Root cause of LEDs not updating - ARM global constructors run before hardware init, so pinMode() in ShifterStr constructor silently fails
type: project
---

On Arduino UNO R4 WiFi (ARM Cortex-M4), global C++ constructors execute before the hardware abstraction layer is initialised. The ShifterStr library called pinMode() in its constructor, which silently failed on the R4 -- leaving all shift register pins (2,3,4,5,6,7) in INPUT mode. This meant shiftOut() and digitalWrite() had no effect, so no data reached the TPIC6B595 chips and no LEDs changed.

**Why:** The original Arduino UNO (AVR ATmega328P) does not have this issue because AVR GPIO is controlled by direct register writes that work immediately. ARM boards require peripheral clock enable and pin mux configuration that happens during init().

**How to apply:** Any Arduino library that calls pinMode() or digitalWrite() in a global constructor will break on the R4 WiFi. Always provide a begin() method and call it from setup(). This applies to ShifterStr and any future libraries.

Fix applied 2026-03-20: Added begin() method to ShifterStr, removed pinMode() from constructor, added begin() calls in scoreboard.ino setup().
