#include "Arduino.h"
#include "NovaDotMatrixDriver.h"
#include "NovaDotMatrixCommands.h"


uint8_t get_random_graph();

void NovaDotMatrixDriver::Setup(void) {
  pinMode(clk_pin,OUTPUT);
  pinMode(data_pin,OUTPUT);
}



void NovaDotMatrixDriver::Write(uint8_t val) {
  // send one byte to the blinky

  uint8_t i,bitmask;

  bitmask = 0b10000000; // starting with the MSB
  noInterrupts();
  for ( i = 0; i < 8  ; i++ )
  {
    if (val & bitmask)
      digitalWrite(data_pin,1);
    else
      digitalWrite(data_pin,0);


    digitalWrite(clk_pin,1);
    delayMicroseconds(NDM_HALF_BIT_PERIOD_US);

    digitalWrite(clk_pin,0);
    delayMicroseconds(NDM_HALF_BIT_PERIOD_US);


    bitmask = bitmask >> 1; // next bit
  }
  digitalWrite(data_pin,0);
  interrupts();

  delay(NDM_INTERCMD_DELAY_MS);

}


void NovaDotMatrixDriver::WriteBuf(uint8_t *buf, uint8_t len) {
  while(len--) {
    Write(*buf++);
  }
}

