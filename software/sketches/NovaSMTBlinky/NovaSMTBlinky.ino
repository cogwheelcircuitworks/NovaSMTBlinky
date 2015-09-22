#include <ATtinyTimer.h>
#include <NovaSMTBlinky.h>

/*
Basic code to make the Nova Surface-Mount Blinky do something

Using SparkFun ATtiny programmer

https://learn.sparkfun.com/tutorials/tiny-avr-programmer-hookup-guide/

Set Board->ATtiny
Set Processor->ATtiny85
Set Clock Speed->8Mhz Internal
Set Programmer->USBTinyISP

May have just been me but was getting ld.exe to crash all the time. Discussion and fix here: 
https://forum.arduino.cc/index.php?topic=313442.0

*/
SMTBlinky smtblinky;
ATtinyTimer attinytimer;
void setup() {
  smtblinky.Setup();
  attinytimer.Setup();
  smtblinky.SetScrollingText("9876543210");
}

void loop() {
   
  smtblinky.Loop(SMTBlinky::LoopModeScrollMessage);

}
