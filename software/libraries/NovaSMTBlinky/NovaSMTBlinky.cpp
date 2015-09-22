/*
 Demonstration software for the ATTiny-based Nova Labs SMT Practice Board

 Sample Sketch:

#include <NovaSMTBlinky.h>

SMTBlinky smtblinky;

void setup() {
  smtblinky.Setup();
}

void loop() {
  // put your main code here, to run repeatedly:
  smtblinky.Loop();
}

*/


#include "NovaSMTBlinky.h" // pull in class definitions
#include "Arduino.h" // pull in regular Arduino cruft
#include "FontAlphaNum57.h"
#include "ATtinytimer.h"


extern ATtinyTimer attinytimer;

void SMTBlinky::Setup(void)
{
#if 1
/* 
   35 LEDs are arranged as 7 rows of 5 columns multiplexed 
   LEDs can be turned on only one row at a time
   Go through all rows faster than a 30th of a second and it 
   will look like they are all on
  
   We have two 8 bit Shift Registers in serial arranged as:

   |15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
   | X| X| X|C4|C3|C2|C1|C0| X|R6|R5|R4|R3|R2|R1|R0|
             ---COLUMN-----    --------ROW---------

   and another pin that is a data..
 */
  pinMode(SMTBLINKY_CLK_PIN,OUTPUT); // pin that clocks data into the shift registers
  pinMode(SMTBLINKY_DAT_PIN,OUTPUT); // pin that is the data being clocked in..
  pinMode(SMTBLINKY_COL_ENABLE_PIN,OUTPUT); // pin that turns off the display while we are shifting

  digitalWrite(SMTBLINKY_DAT_PIN,1);
  digitalWrite(SMTBLINKY_CLK_PIN,1);
  digitalWrite(SMTBLINKY_COL_ENABLE_PIN,1);

  scrollrate_div = scrollrate_ctr = SMTBLINKY_SCROLLRATE_VAL;

  dwell_div = dwell_ctr =  SMTBLINKY_DWELL_VAL;

  col_ctr = scrollstep = 0;

#endif
}


void SMTBlinky::Loop(unsigned char loop_mode)
{
  // called continuously from main Loop() 
  // Take care of all the dynamic operations needed to run the display
  // Depends on the attinytimer library
  unsigned char i,k;
  static unsigned char rowdat = 1;
  static char charindex = '0' - 32;
  static unsigned char colno = 0;

  /* everything is timed. We don't do anything more frequently than 
   * 1/200th of a second.. We watch flags set by a periodic
   * interrupt (attinytimer), which will sequence all the necessary events
   */

  if (!attinytimer.TwoHundredHzFlag) 
    return;
  
  // This is the top loop which sets the basic behavior. 
  // It is expected that this is called at 200hz (50ms) rate
  // Each column is refreshed at this rate.
  // The entire display gets refreshed at 5x this rate or 250ms
  // which is fast enough that the display won't flicker

  switch(loop_mode) {

    case  LoopModeScrollMessage:
      // All the work is done in WriteCol()
      WriteNextCol();
      break;

    case  LoopModeNumbers:
      // Just a test. Loop on numbers.

      rowdat = pgm_read_byte(font_5x7_2 + (charindex*5) + colno);
      WriteCol(colno, rowdat); // write the data out

      colno++; // update column number
      if (colno > 4) // if we've done 5 columns, go back to col 0
        colno = 0;

      if (attinytimer.OneHzFlag) { 
        // once every second, update the charindex to point to the next 
        // character in the table
        attinytimer.OneHzFlag = false;
        charindex++;
        if (charindex > '9' - 32 || charindex < '0' - 32)
          // make sure we do just numbers
          charindex = '0' - 32 ;
      }

      break;

    case  LoopModeAllChars:
      // Works like LoopModeNumbers except we don't confine ourselves to just numbers
      rowdat = pgm_read_byte(font_5x7_2 + (charindex*5) + colno);
      WriteCol(colno, rowdat);

      colno++;
      if (colno > 4)
        colno = 0;

      if (attinytimer.OneHzFlag) {
        attinytimer.OneHzFlag = false;
        charindex++;
        if (charindex >= 95)
          charindex = 0;
      }

      break;

    case  LoopModePattern:
      // Basic test pattern
      WriteCol(colno, rowdat); // write current column data out

      if (attinytimer.TenHzFlag) { // 10x per second, change the test pattern
        rowdat |= (rowdat << 1);
        if (rowdat == 0xff) // if we are all 1's, then revert to all zeroes
          rowdat = 1;
      }

      if (attinytimer.OneHzFlag) {
        rowdat |= (rowdat << 1);
        if (rowdat == 0xff)
          rowdat = 1;
      }

      colno++;
      if (colno > 4)
        colno = 0;
      break;
      

    default:
      break;
  }

  if (attinytimer.OneHzFlag) 
    attinytimer.OneHzFlag = false; 

  if (attinytimer.TenHzFlag)
    attinytimer.TenHzFlag = false;

  if (attinytimer.OneHundredHzFlag)
    attinytimer.OneHundredHzFlag = false;

  if (attinytimer.TwoHundredHzFlag)
    attinytimer.TwoHundredHzFlag = false;

  return;
}




#if 1
void SMTBlinky::WriteCol(unsigned char colno,unsigned char rowdat) {

  unsigned char mask,colbit;

  //cli();

  mask   = 0b00010000;
  colbit = 0b00000001 << (4-colno);

  // column
  unsigned char i;

  PORTB |= SMTBLINKY_COL_ENABLE_BIT; // disable all columns by shutting off their current
  // column bits first
  for (i = 0;i < 5; i++) {

    // set data bit to 0 or 1
    if (mask & colbit) {
      PORTB &= ~SMTBLINKY_DAT_BIT; 
    }
    else  {
      PORTB |= SMTBLINKY_DAT_BIT; 
    }

    // toggle clock
    PORTB &= ~SMTBLINKY_CLK_BIT; 
    PORTB |= SMTBLINKY_CLK_BIT; 

    mask = mask >> 1;

  }
  // extra clock for unused bit..
  PORTB &= ~SMTBLINKY_CLK_BIT; 
  PORTB |= SMTBLINKY_CLK_BIT; 

  // row
  mask = 0b01000000;
  for (i = 0;i < 7; i++) {
    unsigned char portpat1,portpat2;

    if (mask & rowdat) {
      PORTB |= SMTBLINKY_DAT_BIT; 
    }
    else  {
      PORTB &= ~SMTBLINKY_DAT_BIT; 
    }

    PORTB &= ~SMTBLINKY_CLK_BIT;  
    PORTB |= SMTBLINKY_CLK_BIT; 

    mask = mask >> 1;
  }
  PORTB &= ~SMTBLINKY_COL_ENABLE_BIT; // enable column drivers

  //sei();

}
#endif

void SMTBlinky::SetScrollingText(char *cp) {
  txt_headp = txt_curp = cp; 
}

void *SMTBlinky::WriteNextCol(void) {
  // Called during multiplexing to write out the next column of character data
  // Implements scrolling
  static unsigned char lastscrollstep;
  char *txt_nextp, space;

  space = ' ';

  txt_nextp = txt_curp + 1;
  if (!(*txt_nextp))
    txt_nextp = &space;



  if (lastscrollstep != scrollstep) {

    switch (scrollstep) {

      case 0:
        coldata[0] = pgm_read_byte( (font_5x7_2 + (*txt_curp - 32) * 5) + 0 );
        coldata[1] = pgm_read_byte( (font_5x7_2 + (*txt_curp - 32) * 5) + 1 );
        coldata[2] = pgm_read_byte( (font_5x7_2 + (*txt_curp - 32) * 5) + 2 );
        coldata[3] = pgm_read_byte( (font_5x7_2 + (*txt_curp - 32) * 5) + 3 );
        coldata[4] = pgm_read_byte( (font_5x7_2 + (*txt_curp - 32) * 5) + 4 );


        dwell_ctr = dwell_div;

        break;

      case 1:
        coldata[0] = coldata[1];
        coldata[1] = coldata[2];
        coldata[2] = coldata[3];
        coldata[3] = coldata[4];
        coldata[4] = 0; 


        break;


      case 2:
        coldata[0] = coldata[1];
        coldata[1] = coldata[2];
        coldata[2] = coldata[3];
        coldata[3] = 0;
        coldata[4] = 0; 
        break;

      case 3:
        coldata[0] = coldata[1];
        coldata[1] = coldata[2];
        coldata[2] = 0;
        coldata[3] = 0;
        coldata[4] = pgm_read_byte( (font_5x7_2 + (*txt_nextp - 32) * 5) + 0 );

        break;

      case 4:
        coldata[0] = coldata[1];
        coldata[1] = 0;
        coldata[2] = 0;
        coldata[3] = coldata[4];
        coldata[4] = pgm_read_byte( (font_5x7_2 + (*txt_nextp - 32) * 5) + 1 );

        break;

      case 5:
        coldata[0] = 0;
        coldata[1] = 0;
        coldata[2] = coldata[3];
        coldata[3] = coldata[4];
        coldata[4] = pgm_read_byte( (font_5x7_2 + (*txt_nextp - 32) * 5) + 2 );
        break;

      case 6:
        coldata[0] = 0;
        coldata[1] = coldata[2];
        coldata[2] = coldata[3];
        coldata[3] = coldata[4];
        coldata[4] = pgm_read_byte( (font_5x7_2 + (*txt_nextp - 32) * 5) + 3 );
        txt_curp++;
        if (!(*txt_curp))
          txt_curp = txt_headp;
        break;

      default:
        break; 

    }

  }
  lastscrollstep = scrollstep;

  WriteCol(col_ctr,coldata[col_ctr]);

  col_ctr++;
  if (col_ctr > 4)
    col_ctr = 0;

  if (dwell_ctr) {
    dwell_ctr--;
  }
 
  if (!dwell_ctr && !(--scrollrate_ctr)) { 

    scrollstep++;
    if (scrollstep > 6)
      scrollstep = 0;

    scrollrate_ctr = scrollrate_div;
  }

}
