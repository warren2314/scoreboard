/*
  ShifterStr.h - Library
  Code for driving multiple common Anode 7 Segment Displays using the TI TPIC6B595 8-Bit Shift Register
  Originally written for Arduino UNO (AVR). Updated for R4 WiFi (ARM) compatibility.
  25.02.2011
  Released into the public domain.

  Connect between shift registers:-
  SRCLRPin, SRCKPin, RCKPin and connect the SEROUT to SERIN of each cascaded shift register
  Make sure the G Pin is grounded
  Make sure SRCLR is tied to 5V

  Updated by IN to accept string input, to get around arduino 65k integer limit
  Updated 2026: Added begin() for ARM compatibility (R4 WiFi)
*/

#ifndef ShifterStr_h
#define ShifterStr_h

#include "Arduino.h"

class Shifter
{
  public:
    Shifter(int NumOfDigits, int SRCKPin, int SERINPin, int RCKPin);
    void begin();
    void setPulseDelayMs(unsigned int pulseDelayMs);
    void clear();
    int display(const char* numberToDisplay);

  private:
    void shiftByteSlow(uint8_t value);
    void latch();
    int _NumOfDigits;
    int _SRCKPin;
    int _SERINPin;
    int _RCKPin;
    unsigned int _pulseDelayMs;

};

#endif
