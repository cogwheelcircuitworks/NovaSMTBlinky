/*
 * Drive a Nova SMT Blinky LED dot matrix board from an Arduino 
 */

#include "Arduino.h"
#include <NovaDotMatrixCommands.h>
#include <NovaDotMatrixDriver.h>

NovaDotMatrixDriver novadotmatrixdriver;

static  unsigned long cur_ms;
static  unsigned long last_ms;
static  bool demo_complete;
char c;

void setup() {
    novadotmatrixdriver.clk_pin = 7; // select clock pin
    novadotmatrixdriver.data_pin = 8; // select data pin
    novadotmatrixdriver.Setup();

    Serial.begin(9600); // tell outside world what we are doing

    c = 'A';
    Serial.println("Demo start");
    randomSeed(analogRead(0));

    // will cause the inter-demo reset to fire
    last_ms = 0;
    demo_complete = true;
}

void loop() {
  //
  // put the nova dot matrix blinky through its paces
  //

#define START_DEMO 1
#define END_DEMO 6

#define MAX_DEMO 6 // always the actual # of demos
  static uint8_t       which_demo    = START_DEMO;
  static bool          did_this_once = false;
  static unsigned long demo_duration = NDM_DEMO_DURATION_MS;
  static uint8_t dat[5]; // for data to send
  static uint8_t d;
  uint8_t get_random_graph();

  static int8_t inner_demo_ctr       = 0; // inner counter for timing modulations within a demo
  static int8_t inner_demo_step      = 1; // they are signed so we can add/subtract


  cur_ms = millis();

  // 
  // Run a demo
  //
  switch(which_demo) {

    case 0: 
      if (!did_this_once) {
        Serial.println("Alternate Font");
        novadotmatrixdriver.Write(ndotm_cmd_escape_code);
        novadotmatrixdriver.Write(ndotm_cmd_font);
        novadotmatrixdriver.Write(1); 
        c = '0';
      }
      did_this_once = true;
      novadotmatrixdriver.Write((uint8_t) c);
      c++;
      if (c > '}') {
        c = ' ';
        demo_complete = true;
      }
      delay(250);
      break;

    case 1: 
      // Raw Bits
      // Crawling bit
      if (!did_this_once) {
        Serial.println("Load Raw Data.. Crawling bit..");
        did_this_once = true;
        d = 0;
      }
      dat[0] = (1 << (d-0));
      dat[1] = (1 << (d-7));
      dat[2] = (1 << (d-14));
      dat[3] = (1 << (d-21));
      dat[4] = (1 << (d-28));

      novadotmatrixdriver.Write(ndotm_cmd_escape_code);
      novadotmatrixdriver.Write(ndotm_cmd_data);
      for(uint8_t i = 0; i < 5; i++) 
        novadotmatrixdriver.Write(dat[i]);

      d++;
      if (d > 35) {
        demo_complete = true;
        d = 0;
      }
      break;


    case 2: 
      // One new column of data, the rest scroll left 
      if (!did_this_once) {
        Serial.println("Load raw data and scroll..");
        did_this_once = true;
        inner_demo_ctr = 100;
        demo_complete = false;
        d = 0;
      }

      // get one column of data to and display will scroll contents
      novadotmatrixdriver.Write(ndotm_cmd_escape_code);
      novadotmatrixdriver.Write(ndotm_cmd_data_scroll);
      novadotmatrixdriver.Write(get_random_graph());

      if (!(--inner_demo_ctr)) {
        inner_demo_ctr = 100;
        Serial.print(demo_complete,BIN); Serial.println("");

        // periodically change the shift direction left, right, up down.
        novadotmatrixdriver.Write(ndotm_cmd_escape_code);
        novadotmatrixdriver.Write(ndotm_cmd_shift_dir);
        novadotmatrixdriver.Write(d);

        if (++d > 3) {
          // when we've been through every direction quit
          d = 0;
          demo_complete = true;
        }

      }

      delay(30);

      break;

    case 3:
      // ------------------
      // scrolling message
      if (!did_this_once) {
        Serial.println("Scrolling Message");
        novadotmatrixdriver.Write(ndotm_cmd_escape_code);
        novadotmatrixdriver.Write(ndotm_cmd_message);
        novadotmatrixdriver.WriteBuf((uint8_t *)"01234 ",7); // send a max of 32, null terminate
        demo_duration = 10000;
      }
      did_this_once = true;
      delay(1000);
      Serial.println("Scrolling Message (con't)");
      demo_complete = true;
      break;

      //Serial.print(dat[4],BIN); Serial.println("");

      novadotmatrixdriver.Write(ndotm_cmd_escape_code);
      novadotmatrixdriver.Write(ndotm_cmd_data);
      for(uint8_t i = 0; i < 5; i++) 
        novadotmatrixdriver.Write(dat[i]);

      delay(5);
      break;

    case 4: 
      // ------------------------
      // send a range of characters
      if (!did_this_once) {
        Serial.println("Sequential Characters");
        c = '0';
      }
      did_this_once = true;
      novadotmatrixdriver.Write((uint8_t) c);
      c++;
      if (c > '}') {
        c = ' ';
        demo_complete = true;
      }
      delay(250);
      break;

    case 5: 
      // -------------------------
      // Random Raw Data
      if (!did_this_once) {
        Serial.println("Random Raw Data");
        did_this_once = true;
      }
      for(uint8_t i = 0; i < 5; i++)  {
        dat[i] =  (uint8_t)random(255);
      }

      novadotmatrixdriver.Write(ndotm_cmd_escape_code);
      novadotmatrixdriver.Write(ndotm_cmd_data);
      for(uint8_t i = 0; i < 5; i++) 
        novadotmatrixdriver.Write(dat[i]);

      break;

    case MAX_DEMO:
      // -------------------------
      if (!did_this_once) {
        Serial.println("Two Characters");
        did_this_once = true;

        novadotmatrixdriver.Write(ndotm_cmd_escape_code);
        novadotmatrixdriver.Write(ndotm_cmd_transition);
        novadotmatrixdriver.Write(0);
      }

      novadotmatrixdriver.Write(ndotm_cmd_escape_code);
      novadotmatrixdriver.Write(ndotm_cmd_2ch);

      if (inner_demo_ctr++ > 99)
        inner_demo_ctr = 0;

      delay(100);

      // tens digit
      c = (inner_demo_ctr/10) + '0'; // turn it into ascii
      novadotmatrixdriver.Write(c);

      // ones digit
      c = (inner_demo_ctr%10) + '0'; // turn it into ascii
      novadotmatrixdriver.Write(c);

      break;




    default:
      break;
  }

  // 
  // Change to next demo
  //
  if ((cur_ms >= (last_ms + demo_duration)) && demo_complete) {

    Serial.print("Reset...");
    novadotmatrixdriver.Write(ndotm_cmd_escape_code);
    novadotmatrixdriver.Write(ndotm_cmd_reset);

    // Periodically change to next demo
    last_ms = cur_ms;

    which_demo++;
    if (which_demo > END_DEMO) {
      which_demo = START_DEMO;
    }
    Serial.println("---------------------");
    Serial.print("Demo #"); Serial.print(which_demo,DEC); Serial.print(" ");

    did_this_once = false;

  }


}




uint8_t get_random_graph() {

  static uint8_t shift_amount = 0;
  static uint8_t d = 0;
  uint8_t i;

  d = 0;
  for (i = 0; i < shift_amount; i++){
    d |= (1 << i);
  }

  if (random(255) <= 128) {
    if (shift_amount)
      shift_amount--;
  } else {
    if (shift_amount < 6)
      shift_amount++;
  }
  return d;

}


