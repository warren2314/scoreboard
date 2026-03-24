//////////////////////////////////////////////////////////////
//
// Name: scoreBoard.pde
// Author: Ian Nice - ian@woscc.org.uk
// Version: 0.1 - 14/03/2013 - First version for testing
//          0.2 - 01/04/2013 - Updated to produce additional debug
//          0.3 - 06/04/2013 - Updated to simplify wiring and fix bug
//          0.4 - 07/04/2013 - Updated to use strings instead of integers
//          0.5 - 11/04/2013 - Changed to simplify wiring setup, now that i dont have limitation of Integer size.
//          0.6 - 02/07/2013 - Updated because ted doesnt like preceeding zeros :-)
//          0.7 - 20/02/2016 - Updated to work with Arduino IDE 1.6.7 - Thanks to James W @ Potton Town CC for helping debug the problems
//
// Acknowledgement:
//  shifter.h - http://www.proto-pic.com
//  CmdMessenger.h - https://github.com/dreamcat4/cmdmessenger
//  Streaming.h - http://arduiniana.org/libraries/streaming/
//  Base64.h - https://github.com/adamvr/arduino-base64
//  Where the idea began.......
//  http://www.fritz-hut.com/arduinopi-web-based-controller-for-arduino/
//
//////////////////////////////////////////////////////////////

// Command overview
// 4,BatAScore,Total,BatBScore,Wickets,Overs,Target# //display a score on the board
// 5# //test mode

// Wiring layout
// shifter set 1, pins 2 (SRCK),3 (SERIN),4 (RCK) --> Batsman 1 score, total, batsman b score, 9 characters (e.g. 001200050)
// shifter set 2, pins 5 (SRCK),6 (SERIN),7 (RCK) --> wickets, overs, target, 6 characters (e.g. 240240)


// Shifter library available from http://www.proto-pic.com/Resources/shifter.zip
#include <ShifterStr.h>

// CmdMessenger library available from https://github.com/dreamcat4/cmdmessenger
#include <CmdMessenger.h>

// Base64 library available from https://github.com/adamvr/arduino-base64
#include <Base64.h>

// Streaming4 library available from http://arduiniana.org/libraries/streaming/
#include <Streaming.h>

// set up the field seperators and command seperators.  
char field_separator = ',';
char command_separator = '#';

// set up the chharacter arrays we will use later
char batsmanAScore[4] = { '\0' };
char batsmanBScore[4] = { '\0' };
char total[4] = { '\0' };
char wickets[2] = { '\0' };
char overs[3] = { '\0' };
char target[4] = { '\0' };

// set up the headings 
char batAHeading[] = "Batsman A Score:";
char batBHeading[] = "Batsman B Score:";
char totalHeading[] = "Total Score:";
char oversHeading[] = "Overs:";
char wicketsHeading[] = "Wickets:";
char targetHeading[] = "Target:";
char batAandTotal[] = "Batsman A Score, Total and Batsman B Score:";
char wicketsOversandTarget[] = "Wickets, Overs and Target:";

// Attach a new CmdMessenger object to the default Serial port
CmdMessenger cmdMessenger = CmdMessenger(Serial, field_separator, command_separator);

// dreamcat4 default
// ------------------ C M D  L I S T I N G ( T X / R X ) ---------------------

// We can define up to a default of 50 cmds total, including both directions (send + recieve)
// and including also the first 4 default command codes for the generic error handling.
// If you run out of message slots, then just increase the value of MAXCALLBACKS in CmdMessenger.h

// Commands we send from the Arduino to be received on the PC
enum
{
  kCOMM_ERROR    = 000, // Lets Arduino report serial port comm error back to the PC (only works for some comm errors)
  kACK           = 001, // Arduino acknowledges cmd was received
  kARDUINO_READY = 002, // After opening the comm port, send this cmd 02 from PC to check arduino is ready
  kERR           = 003, // Arduino reports badly formatted cmd, or cmd not recognised

  // Now we can define many more 'send' commands, coming from the arduino -> the PC, eg
  // kICE_CREAM_READY,
  // kICE_CREAM_PRICE,
  // For the above commands, we just call cmdMessenger.sendCmd() anywhere we want in our Arduino program.

  kSEND_CMDS_END, // Must not delete this line
};

// set up 3 groups of shifters
Shifter shifterSet1(9, 2, 3, 4);  //Bat A, Total, Bat B controlled by pins 2,3 and 4
Shifter shifterSet2(6, 5, 6, 7);  //Wickets, Overs and Target controlled by pins 5,6 and 7


void update_scoreboard()
{
  while ( cmdMessenger.available() )
  {
    
    // lets assume we always get the correctly formatted string
    // its lazy, but should be ok
    
    char buf[4] = { '\0' };
    char tempString[10] = { '\0' }; 
    
    // Batsman A Score
    cmdMessenger.copyString(buf, 4);
    strcpy(batsmanAScore,buf);
    cmdMessenger.sendCmd(kACK, batAHeading);
    cmdMessenger.sendCmd(kACK, batsmanAScore);
    
    // Total Score
    memset(buf, '\0', 4);
    cmdMessenger.copyString(buf, 4);
    strcpy(total,buf);
    cmdMessenger.sendCmd(kACK, totalHeading);
    cmdMessenger.sendCmd(kACK, total);
    
    // Batsman B Score
    memset(buf, '\0', 4);
    cmdMessenger.copyString(buf, 4);
    strcpy(batsmanBScore,buf);
    strcpy(tempString,batsmanAScore);
    strcat(tempString,total);
    strcat(tempString,batsmanBScore);
    shifterSet1.display(tempString);
    cmdMessenger.sendCmd(kACK, batBHeading);
    cmdMessenger.sendCmd(kACK, batsmanBScore);
    cmdMessenger.sendCmd(kACK, batAandTotal);
    cmdMessenger.sendCmd(kACK, tempString);
    
    // Wickets
    memset(buf, '\0', 4);
    cmdMessenger.copyString(buf, 4);
    strcpy(wickets,buf);
    cmdMessenger.sendCmd(kACK, wicketsHeading);
    cmdMessenger.sendCmd(kACK, wickets);
    
    // Overs
    memset(buf, '\0', 4);
    cmdMessenger.copyString(buf, 4);
    strcpy(overs,buf);
    cmdMessenger.sendCmd(kACK, oversHeading);
    cmdMessenger.sendCmd(kACK, overs);
    
    // Target
    memset(buf, '\0', 4);
    memset(tempString, '\0', 10);
    cmdMessenger.copyString(buf, 4);
    strcpy(target,buf);
    strcpy(tempString,wickets);
    strcat(tempString,overs);
    strcat(tempString,target);
    shifterSet2.display(tempString);
    cmdMessenger.sendCmd(kACK, targetHeading);
    cmdMessenger.sendCmd(kACK, target);
    cmdMessenger.sendCmd(kACK, wicketsOversandTarget);
    cmdMessenger.sendCmd(kACK, tempString);
  }
}

void test_mode()
{
    char buf[] = { "Test Mode" };
    cmdMessenger.sendCmd(kACK, buf);
    shifterSet1.display("111222333");
    shifterSet2.display("444555");
}

// default dreamcat4 stuff

// ------------------ D E F A U L T  C A L L B A C K S -----------------------

void arduino_ready()
{
  // In response to ping. We just send a throw-away Acknowledgement to say "im alive"
  cmdMessenger.sendCmd(kACK,"Arduino ready");
}

void unknownCmd()
{
  // Default response for unknown commands and corrupt messages
  cmdMessenger.sendCmd(kERR,"Unknown command");
}

// ------------------ E N D  C A L L B A C K  M E T H O D S ------------------

// Commands we send from the PC and want to recieve on the Arduino.
// We must define a callback function in our Arduino program for each entry in the list below vv.
// They start at the address kSEND_CMDS_END defined ^^ above as 004
messengerCallbackFunction messengerCallbacks[] = 
{
  update_scoreboard,            // 004
  test_mode,                    // 005
  NULL
};
// Its also possible (above ^^) to implement some symetric commands, when both the Arduino and
// PC / host are using each other's same command numbers. However we recommend only to do this if you
// really have the exact same messages going in both directions. Then specify the integers (with '=')


// ------------------ S E T U P ----------------------------------------------

void attach_callbacks(messengerCallbackFunction* callbacks)
{
  int i = 0;
  int offset = kSEND_CMDS_END;
  while(callbacks[i])
  {
    cmdMessenger.attach(offset+i, callbacks[i]);
    i++;
  }
}

void setup()
{
  // Listen on serial connection for messages from the pc
  Serial.begin(57600); // Arduino Uno, Mega, with AT8u2 USB

  // cmdMessenger.discard_LF_CR(); // Useful if your terminal appends CR/LF, and you wish to remove them
  cmdMessenger.print_LF_CR();   // Make output more readable whilst debugging in Arduino Serial Monitor
  
  // Attach default / generic callback methods
  cmdMessenger.attach(kARDUINO_READY, arduino_ready);
  cmdMessenger.attach(unknownCmd);

  // Attach my application's user-defined callback methods
  attach_callbacks(messengerCallbacks);

  arduino_ready();
  
  // Set the displays to 000 on boot
  //shifterSet1.display("000000000");
  //shifterSet2.display("000000");
  shifterSet1.display("--0--0--0");
  shifterSet2.display("0-0--0");
}

// ------------------ M A I N ( ) --------------------------------------------

// Timeout handling
long timeoutInterval = 2000; // 2 seconds
long previousMillis = 0;
int counter = 0;

void timeout()
{
  // blink
  if (counter % 2)
    digitalWrite(13, HIGH);
  else
    digitalWrite(13, LOW);
  counter ++;
}  

void loop()
{
  
  // Process incoming serial data, if any
  cmdMessenger.feedinSerialData();

  // handle timeout function, if any
  if (  millis() - previousMillis > timeoutInterval )
  {
    timeout();
    previousMillis = millis();
  }
  
}
