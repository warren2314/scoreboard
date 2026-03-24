/*
  ShifterStr.cpp - Library
  Code for driving multiple common Anode 7 Segment Displays using the TI TPIC6B595 8-Bit Shift Register
  Originally written for Arduino UNO (AVR). Updated for R4 WiFi (ARM) compatibility.
  25.02.2011
  Released into the public domain.

  Connect between shift registers:-
  SRCLRPin, SRCKPin, RCKPin and connect the SEROUT to SERIN of each cascaded shift register
  Make sure the G Pin is grounded
  Make sure SRCLR is tied to 5V

  Updated by IN to accept string input to get around arduino 65k integer limit
  Updated 2026: Added begin() for ARM compatibility (R4 WiFi).
    On ARM boards (like R4 WiFi), global constructors run BEFORE the hardware
    is fully initialised, so pinMode() calls in the constructor silently fail.
    Call begin() from setup() to configure pins at the right time.
*/

#include "Arduino.h"
#include "ShifterStr.h"

namespace {
const unsigned int DEFAULT_SHIFT_PULSE_MS = 20;
}

Shifter::Shifter(int NumOfDigits, int SRCKPin, int SERINPin, int RCKPin)
{
  _NumOfDigits = NumOfDigits;
  _SRCKPin = SRCKPin;
  _SERINPin = SERINPin;
  _RCKPin = RCKPin;
  _pulseDelayMs = DEFAULT_SHIFT_PULSE_MS;
  // NOTE: Do NOT call pinMode() here.
  // On ARM-based boards (Arduino R4 WiFi), the GPIO hardware may not be
  // ready yet when global constructors run. Call begin() from setup().
}

void Shifter::begin()
{
  pinMode(_SRCKPin, OUTPUT);
  pinMode(_SERINPin, OUTPUT);
  pinMode(_RCKPin, OUTPUT);
  digitalWrite(_SRCKPin, LOW);
  digitalWrite(_SERINPin, LOW);
  digitalWrite(_RCKPin, LOW);
}

void Shifter::setPulseDelayMs(unsigned int pulseDelayMs)
{
  _pulseDelayMs = pulseDelayMs;
}

void Shifter::shiftByteSlow(uint8_t value)
{
  for (uint8_t bit = 0; bit < 8; bit++)
  {
    digitalWrite(_SRCKPin, LOW);
    delay(_pulseDelayMs);
    digitalWrite(_SERINPin, (value & 0x01) ? HIGH : LOW);
    delay(_pulseDelayMs);
    digitalWrite(_SRCKPin, HIGH);
    delay(_pulseDelayMs);
    value >>= 1;
  }

  digitalWrite(_SRCKPin, LOW);
  delay(_pulseDelayMs);
  digitalWrite(_SERINPin, LOW);
}

void Shifter::latch()
{
  delay(_pulseDelayMs);
  digitalWrite(_RCKPin, HIGH);
  delay(_pulseDelayMs);
  digitalWrite(_RCKPin, LOW);
  delay(_pulseDelayMs);
}

void Shifter::clear()
{
  int x;
  digitalWrite(_RCKPin, LOW);
  // shift out 0's to clear display
  for (x = _NumOfDigits - 1; x >= 0; x--)
    {
      shiftByteSlow(0);
    }
  latch();
}

int Shifter::display(const char* numberToDisplay)
{ // Displays number on display
  int SegmentArray[] = {252, 96, 218, 242, 102, 182, 190, 224, 254, 230};
  int x;
  int res;
  int shiftword;

  digitalWrite(_RCKPin, LOW);
  for (x = _NumOfDigits - 1; x >= 0; x--)
  {
	//check if it is a dash, which means digit off
	if (numberToDisplay[x] == '-'){
		//turnoff the digit
  		shiftword = 0;
	} else {
		//turn the character in to an int
  		res = numberToDisplay[x] - '0';
		//look up the bit pattern to display
  		shiftword = SegmentArray[res];
	}

  	// shift out the bits
  	shiftByteSlow(shiftword);
  }
  latch();
  return 0;
}
