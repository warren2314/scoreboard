/*
  ShifterStr.cpp - Library
  Code for driving multiple common Anode 7 Segment Displays using the TI TPIC6B595 8-Bit Shift Register
  This has been written for use with the Arduino UNO. Roy Fisher
  25.02.2011
  Released into the public domain.

  Connect between shift registers:-
  SRCLRPin, SRCKPin, RCKPin and connect the SEROUT to SERIN of each cascaded shift register
  Make sure the G Pin is grounded
  Make sure SRCLR is tied to 5V

  Updated by IN to accept string input to get around arduino 65k integer limit
*/

#include "Arduino.h"
#include "ShifterStr.h"

Shifter::Shifter(int NumOfDigits, int SRCKPin, int SERINPin, int RCKPin)
{
  _NumOfDigits = NumOfDigits;
  pinMode(SRCKPin, OUTPUT);
  _SRCKPin = SRCKPin;
  pinMode(SERINPin, OUTPUT);
  _SERINPin = SERINPin;
  pinMode(RCKPin, OUTPUT);
  _RCKPin = RCKPin;    
}

void Shifter::clear()
{
  int x; 
  digitalWrite(_RCKPin, LOW);  
  // shift out 0's to clear display
  for (x = _NumOfDigits; x >= 0; x--)
    {
      shiftOut(_SERINPin, _SRCKPin, LSBFIRST, 0); 
    }
  digitalWrite(_RCKPin, HIGH);
}

int Shifter::display(char* numberToDisplay)
{ // Displays number on display
  int SegmentArray[] = {252, 96, 218, 242, 102, 182, 190, 224, 254, 230};
  int x;
  int res;
  int shiftword;
  
  digitalWrite(_RCKPin, LOW);
  for (x = _NumOfDigits ; x >= 0; x--)
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
  	shiftOut(_SERINPin, _SRCKPin, LSBFIRST, shiftword);
  }
  digitalWrite(_RCKPin, HIGH);
}

