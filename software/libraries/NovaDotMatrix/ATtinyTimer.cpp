/*

 Basic ATTiny Timer w/Interrupt code 

 https://learn.adafruit.com/trinket-gemma-blinky-eyes/code
 
 */

#include "Arduino.h"
#include <avr/io.h>      
#include <avr/interrupt.h>
#include "ATtinyTimer.h"
#include "NovaDotMatrix.h"


extern ATtinyTimer attinytimer;

volatile uint8_t ATtinyTimerFastFlags;
volatile uint8_t ATtinyTimerFiveHundredHzCtr;
uint8_t ATtinyTimerFiveHundredHzDiv;

volatile uint8_t ATtinyOneHundredHzFlags;


void ATtinyTimer::Setup(void) { 
  // called once to setup the periodic timer interrupt
  
  //  COUNTERS                  DIVISORS                      INITIALIZERS
  ATtinyTimerFiveHundredHzCtr = ATtinyTimerFiveHundredHzDiv = ATT_FIVE_HUNDRED_HZ_DIV;
  OneHundredHzCtr             = OneHundredHzDiv             = ATT_ONEHUNDRED_HZ_DIV;
  FiftyHzCtr                  = FiftyHzDiv                  = ATT_FIFTY_HZ_DIV;
  TenHzCtr                    = TenHzDiv                    = ATT_TENHZDIV;
  OneHzCtr                    = OneHzDiv                    = ATT_ONEHZDIV;
  // this one outside the object for dereference speed
  ATtinyTimerFastFlags        = OneHzFlags                  = 0b00000000;

  TIMSK &= ~_BV(TOIE1); // Turn this interrupt off

  // See TCCR1 Description in 
  // http://www.atmel.com/Images/atmel-2586-avr-8-bit-microcontroller-attiny25-attiny45-attiny85_datasheet.pdf
  TCCR1 = 0b00000100; // 2 msec

  TIMSK |= _BV(TOIE1);  // Enable Timer/Counter1 Overflow Interrupt

  // sei(); // enable interrupts

}

void ATtinyTimer::Loop(void) {
  // we just set a bunch of flags depending on how much time has elapsed.
  // The main Loop() watches for these and does things a
  // like scanning the multiplex display and scrolling, etc...
  // The main Loop needs to clear these flags
  
  DISABLE_TIMER_IRUPS;
  if (ATtinyTimerFastFlags & ATT_FAST_FLAG_BIT) { 
    // go look what TCCR1 was set to in Setup()
    // to find out what 'fast' is
    ATtinyTimerFastFlags &= ~ATT_FAST_FLAG_BIT;
    ENABLE_TIMER_IRUPS;

    // 500 HZ
    if (ATtinyTimerFiveHundredHzCtr) {
      ATtinyTimerFiveHundredHzCtr--;

      // 100 HZ
      if (!(--OneHundredHzCtr)) {
        OneHundredHzCtr = OneHundredHzDiv;
        OneHundredHzFlag = true;
        
        // 50 HZ
        if (!(--FiftyHzCtr)) {
          FiftyHzCtr = FiftyHzDiv;
          FiftyHzFlag = true;
        }

        // 10 HZ
        if (!(--TenHzCtr)) {
          TenHzCtr = TenHzDiv;
          TenHzFlags |= 0b11111111; 
        }
        // 1 HZ
        if (!(--OneHzCtr)) {
          OneHzCtr = OneHzDiv;
          OneHzFlag = true;
        }
      }
    }
  }
  ENABLE_TIMER_IRUPS;

}

ISR (TIMER1_OVF_vect) {
  // interrupt service routine for Timer0
  // Gets called 1000 times per second.
  //attinytimer.FastFlags |= 0b11111111; // indicate to everyone we've been here
  ATtinyTimerFastFlags |= 0b11111111;
}

