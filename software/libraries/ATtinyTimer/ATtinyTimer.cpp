/*
 Basic ATTiny Timer w/Interrupt code 

 https://learn.adafruit.com/trinket-gemma-blinky-eyes/code
 
 */

#include <Arduino.h>
#include "ATtinyTimer.h"
#include <avr/io.h>      
#include <avr/interrupt.h>

extern ATtinyTimer attinytimer;

void ATtinyTimer::Setup(void) { 
  // called once to setup the periodic timer interrupt
  
  TwoHundredHzCtr   = ATT_TWOHUNDREDHZDIV;
  OneHundredHzCtr = ATT_ONEHUNDREDHZDIV;
  TenHzCtr        = ATT_TENHZDIV;
  OneHzCtr        = ATT_ONEHZDIV;

  TIMSK &= ~_BV(TOIE1); // Turn this interrupt off

  // See TCCR1 Description in 
  // http://www.atmel.com/Images/atmel-2586-avr-8-bit-microcontroller-attiny25-attiny45-attiny85_datasheet.pdf
  
  triggered = false;

  TCCR1 |= _BV(CS10); // should get is 1khz on 8mhz attiny (PCK)
  TIMSK |= _BV(TOIE1);  // Enable Timer/Counter1 Overflow Interrupt

  sei();

}


ISR (TIMER1_OVF_vect) {
  // interrupt service routine for Timer0
  // Gets called 1000 times per second.

  TIMSK &= ~_BV(TOIE1); // I have this thing about shutting off interrupts
  sei();                // ... but only the ones I'm servicing


  // we just set a bunch of flags depending on how much time has elapsed.
  // The main Loop() watches for these and does things a
  // like scanning the multiplex display and scrolling, etc...
  // The main Loop needs to clear these flags

  attinytimer.triggered = true;

  // 200 HZ
  if (!(--attinytimer.TwoHundredHzCtr)) {
    attinytimer.TwoHundredHzCtr = ATT_TWOHUNDREDHZDIV;
    attinytimer.TwoHundredHzFlag = true;
  }

  // 100 HZ
  if (!(--attinytimer.OneHundredHzCtr)) {
    attinytimer.OneHundredHzCtr = ATT_ONEHUNDREDHZDIV;
    attinytimer.OneHundredHzFlag = true;

    // 10 HZ
    if (!(--attinytimer.TenHzCtr)) {
      attinytimer.TenHzCtr = ATT_TENHZDIV;
      attinytimer.TenHzFlag = true;
    }
    // 1 HZ
    if (!(--attinytimer.OneHzCtr)) {
      attinytimer.OneHzCtr = ATT_ONEHZDIV;
      attinytimer.OneHzFlag = true;
    }
  }

  //attinytimer.isrCallback();

  TIMSK |= _BV(TOIE1);
}

