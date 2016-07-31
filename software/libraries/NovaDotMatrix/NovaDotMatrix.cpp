/* 
 Arduino-compatible library for 

 ATTiny-based Nova Labs SMT Dot Matrix Practice Board 

*/

#include "Arduino.h"               // pull in regular Arduino cruft
#include "FontAlphaNum57.h"        // 5x7 fonts
#include "FontAlphaNum35.h"        // 3x5 fonts
#include "avr/interrupt.h"         // we findout about incoming data via interrupts

#include "ATtinytimer.h"           // Interface to ATtiny's timer hardware

#define NDOTM_COMPILE_DEMO         // save space if you don't need the demo mode
//#define NDOTM_FORCEDEMO            // no pin check on reset
#include "NovaDotMatrix.h"         // pull in class definitions

#include "NovaDotMatrixCommands.h" // command our master can send us

extern ATtinyTimer attinytimer;
extern NovaDotMatrix novadotmatrix;


void NovaDotMatrix::Setup(void)
{
  /* 
     35 LEDs are arranged as 7 rows of 5 columns multiplexed 
     LEDs can be turned on only one row at a time
     Sequentially turn on each row within a 30th of a second and to human 
     eyeballs they  will look like they are all on

     display info is stored in coldata[]

              coldata[]
      bitpos  v v v v v 
          v  |0|1|2|3|4|
         [0]:| | | | | |
         [1]:| | | | | |
         [2]:| | | | | |
         [3]:| | | | | |
         [4]:| | | | | |
         [5]:| | | | | |
         [6]:| | | | | |
             +---------+
              | | | | | <edge connector


     We have two 8 bit Shift Registers in serial arranged as:

     |15|14|13|12|11|10|09|08|07|06|05|04|03|02|01|00|
     | X| X| X|C4|C3|C2|C1|C0| X|R6|R5|R4|R3|R2|R1|R0|
     |         ---COLUMN-----    --------ROW---------|

     which are interfaced to the ATtiny via two pins - NDOTM_SR_CLK_PIN, and NDOTM_SR_DAT_PIN
     programmed as outputs

     One additional pin NDOTM_BLANK_DATOUT_PIN is used to turn off the entire display while the new
     bits are being clocked out to the shift registers

     and another pin that is a data..
     */

  pinMode(NDOTM_CLK_IN_PIN    , INPUT);     // clock signal from our master
  pinMode(NDOTM_BLANK_DATOUT_PIN   , OUTPUT);    //

  pinMode(NDOTM_SR_CLK_PIN    , OUTPUT);    // pin that clocks data into the shift registers
  pinMode(NDOTM_SR_DAT_PIN    , OUTPUT);    // pin that is the data being clocked in..
  pinMode(NDOTM_BLANK_DATOUT_PIN, OUTPUT);    // pin that turns off the display while we are shifting

  indata_available = false;                              // input data from master available
  indata_cur_bit   = 7;                                  // current bit from master
  indata           = 0;                                  // current data
  indata_state     = indata_state_norm;
  indata_port      = digitalPinToPort(NDOTM_CLK_IN_PIN);
  indata_idle_ctr  = 0;

  shift_dir        = 0;

  digitalWrite(NDOTM_SR_DAT_PIN,1);
  digitalWrite(NDOTM_SR_CLK_PIN,1);
#if !defined(NDOTM_TESTING)
  // we hijack this pin so we can use it for o-scope debugging
  digitalWrite(NDOTM_BLANK_DATOUT_PIN,1);
#endif

  // set various timer rate defaults

  scroll_rate_div    = scroll_rate_ctr = NDOTM_SCROLLRATE_VAL;
  dwell_div          = dwell_ctr       = NDOTM_DWELL_VAL;
  transition_max     = transition_ctr  = 0;
  col_ctr            = scrollstep      = 0;
  por_ctr            = 0;
  pin_end_is_top     = false;
  flip2char           = false;
  Mode               = ModeReset;
  demo               = false;
  buf_contents       = NDOTM_BUF_CONTENTS_ASCII;
  cur_font           = cur_font_5x7;
  txt_curp = (char *)buf;

  pinMode(NDOTM_DAT_IN_PIN    , INPUT_PULLUP);     // data from our master
  delay(250);// wait a little while for things to settle

#if defined(NDOTM_FORCEDEMO)
  // forget all that checking pins on reset stuff.. 
  demo = true;
#else
  if (digitalRead(NDOTM_DAT_IN_PIN))  {
    // if DAT in high at reset, then we are going to enter demo mode
    PCMSK              &= ~NDOTM_CLK_IN_BIT; // CLK from master causes interrupts
    GIMSK              &= ~0b00100000;      // disable 
    demo = true;
  } else
  {
    // no demo. listen to clk and datain
    PCMSK              |= NDOTM_CLK_IN_BIT; // CLK from master causes interrupts
    GIMSK              |= 0b00100000;      // Enable interrupts
  }
#endif

  // Test each bit is working
  for (uint8_t r = 0; r < NDOTM_NUMROWS; r++) {
    for (uint8_t c = 0; c < NDOTM_NUMCOLS; c++) {
      coldata[c] = (1 << r);
      NDOTM_WRITE_AND_UPDATE_COL_COUNTER;
      delay(5);
    }
  }

  for (uint8_t c = 0; c < NDOTM_NUMCOLS; c++) {
    // write all on as we go into scan
    coldata[c] = 0b01111111;
  }

  attinytimer.Setup();

  sei();
}

#ifdef NDOTM_COMPILE_DEMO
static void DemoManage(void);
static uint8_t GetRandomGraph(void);
#endif

void NovaDotMatrix::Chores(void) {
        ProcessInData();
        attinytimer.Loop();
        ScrollAndDwellManage();
        ProcessInData();
        CommonLoopChores();
}

void NovaDotMatrix::Loop()
{
    /*
       called continuously from main Loop() 
       Take care of all the dynamic operations needed to run the display
       Depends on the attinytimer library

       Everything is timed. We don't do anything more frequently than 
       1/200th of a second.. We watch flags set by a periodic
       interrupt (attinytimer), which will sequence all the necessary events
       This is the top loop which sets the basic behavior. 
       It is expected that this is called at 200hz (50ms) rate
       Each column is refreshed at this rate.
       The entire display gets refreshed at 5x this rate or 250ms
       which is fast enough that the display won't flicker
       */

    switch(Mode) {

      case ModeReset:
        Mode = ModeNorm;
        break;


      case ModeStartTransition:
        Chores();
        if (transition_max != 0) {
          transition_ctr = 0;
          Mode = ModeInTransition;
        } else {
          Mode = ModeNorm;
        }
        break;

      case ModeNorm:
      case ModeStartScrollMessage:
      case ModeScrollMessage:
      case ModeInTransition:
        Chores();
        break;

      default:
        break;
    }


#ifdef NDOTM_COMPILE_DEMO
    if (demo)
      DemoManage();
#endif


}

void NovaDotMatrix::ProcessInData() {
  //
  // Check for incoming data and process it
  //
  static bool last_char_was_esc            = false;
  static uint8_t last_cmd;
  static uint8_t ctr = 0;
  uint8_t c;

#if 1 
  //
  // if they stop send us stuff mid byte, reset our state
  //
  DISABLE_INDATA_IRUPS;
  //if (attinytimer.FastFlags & indata_fast_ctr_flag_bit) {
  if (ATtinyTimerFastFlags & indata_fast_ctr_flag_bit) {
    ATtinyTimerFastFlags &= ~indata_fast_ctr_flag_bit;
    ENABLE_INDATA_IRUPS;


    if (indata_idle_ctr == indata_idle_max - 1) {
      // if idle for a while, reset state
      novadotmatrix.indata_cur_bit = 7;
    }

    if (indata_idle_ctr < indata_idle_max)
      ++indata_idle_ctr;

  }
  ENABLE_INDATA_IRUPS;

#endif

  // another 8 bits arrived. Do something with it
  DISABLE_INDATA_IRUPS;
  if (indata_available) {
    c                = indata; // get collected bits
    indata           = 0; // clear collection area so more can get or'd in
    indata_available = false;  // trigger to ISR to fetch more
    ENABLE_INDATA_IRUPS;

    if (last_char_was_esc) { 
      // if previous character was an escape
      switch(c) {
        // what's character following the escape ?
        case  ndotm_cmd_reset: 
          // Got the command to reset.. reset everything
          pin_end_is_top  = false;
          flip2char       = false;
          dwell_div       = dwell_ctr             = NDOTM_DWELL_VAL;
          scroll_rate_div = scroll_rate_ctr       = NDOTM_SCROLLRATE_VAL;
          transition_ctr  = 0;
          transition_max  = NDOTM_TRANSITION_MAX;
          scrollstep      = 0;
          cur_font        = cur_font_5x7;
          txt_headp       = txt_curp              = (char *)buf;
          shift_dir       = 0;

          // display a blank
          for(ctr = 0; ctr < NDOTM_NUMCOLS; ctr++) {
            buf[ctr] = 0b00000000;
          }
          buf_contents = NDOTM_BUF_CONTENTS_BINARY;
          indata_state = indata_state_norm;
          break; 



        case  ndotm_cmd_flip: // set display upside down
          pin_end_is_top = true;
          break;

        case  ndotm_cmd_noflip: // set display upside down
          pin_end_is_top = false;
          break;

        case  ndotm_cmd_dwell: // set dwell 
          indata_state = indata_state_rx_single_cmd_opcode;
          break;

        case  ndotm_cmd_transition: // set transition
          indata_state = indata_state_rx_single_cmd_opcode;
          break;

        case  ndotm_cmd_rate: // set scroll rate
          indata_state = indata_state_rx_single_cmd_opcode;
          break;

        case  ndotm_cmd_font: // set scroll rate
          indata_state = indata_state_rx_single_cmd_opcode;
          break;

        case  ndotm_cmd_shift_dir: // shift display  
          indata_state = indata_state_rx_single_cmd_opcode;
          break;

        case  ndotm_cmd_2ch: // 2 little characters
        case  ndotm_cmd_2ch_flipped: // 2 little characters 
          indata_state = indata_state_rx_double_cmd_opcode;
          ctr = 0;
          break;

        case  ndotm_cmd_message: 
          indata_state = indata_state_rx_message;
          ctr = 0;
          break;

        case ndotm_cmd_data:
          // the next 5 bytes will be display data
          indata_state = indata_state_rx_data;
          ctr = 0;
          break;

        case ndotm_cmd_data_scroll:
          // accept one byte which is a new column of data
          indata_state = indata_state_rx_data_single_byte_for_scroll;
          ctr = 0;
          break;

        case ndotm_cmd_char:
          break;

        default:
          break;

      }
      last_char_was_esc=false;
      last_cmd = c;
    } else if (c == ndotm_cmd_escape_code) { // if <esc>
      // got command escape. next character interpreted as command
      last_char_was_esc = true;
    } else // any other character besides <esc> or character following <esc> (dealt with above)
    { 
      switch(indata_state) {
        // what to do with next byte
        case indata_state_norm:
          // by default, we just prepare to display the character
          buf[0] = c; // get ascii character.. 
          buf[1] = 0;
          Mode = ModeStartTransition;
          buf_contents = NDOTM_BUF_CONTENTS_ASCII;
          //pin_end_is_top = false; // so transmitted LSB is bottom row
          break;

        case indata_state_rx_message:
          // receive 5 bytes of raw data for display
          if (ctr < 32)
            buf[ctr] = c;

          if (ctr >= 32 || c == 0) {
            buf[ctr] = c;
            buf[ctr+1] = 0;

            txt_headp = txt_curp = (char *)buf;
            buf_contents = NDOTM_BUF_CONTENTS_ASCII;

            indata_state = indata_state_norm;
            Mode = ModeStartScrollMessage;
          }
          ctr++;
          break;

        case indata_state_rx_data:
          // --
          // receive 5 bytes of raw data for display
          switch(ctr) {
            case 0: // receive first byte
            case 1: // receive next byte..
            case 2: // and the next..
            case 3: // and the next..
              buf[4-ctr] = c;
              break;

            case 4: // last one
              buf[4-ctr]         = c;

              indata_state       = indata_state_norm;
              Mode               = ModeNorm;
              buf_contents = NDOTM_BUF_CONTENTS_BINARY;
              pin_end_is_top     = true;              // so transmitted LSB is bottom row

              break;

            default:
              break;
          } 
          if (ctr < NDOTM_NUMCOLS) ctr++;
          break;
          // --


        case indata_state_rx_data_single_byte_for_scroll:
          switch(shift_dir) {
            case 0:
              // shift r->l
              buf[4] = buf[3];
              buf[3] = buf[2];
              buf[2] = buf[1];
              buf[1] = buf[0];
              buf[0] = c;
              break;

            case 1:
              // shift r->l
              buf[0] = buf[1];
              buf[1] = buf[2];
              buf[2] = buf[3];
              buf[3] = buf[4];
              buf[4] = c;
              break;

            case 2:
            /*
             *            source                 dest
             *           coldata[]              coldata[]
             *   bitpos  v v v v v      bitpos  v v v v v
             *       v  |0|1|2|3|4|         v  |0|1|2|3|4|
             *      [0]:|a|b|c|d|e|        [0]:|A|B|C|D|E|
             *      [1]:|f|g|h|i|j|        [1]:|a|b|c|d|e|
             *      [2]:| | | | | |  --->  [2]:|f|g|h|i|j|
             *      [3]:| | | | | |        [3]:| | | | | |
             *      [4]:| | | | | |        [4]:| | | | | |
             *      [5]:| | | | | |        [5]:| | | | | |
             *      [6]:| | | | | |        [6]:| | | | | |
             *          +++++++++++            ++-+-+-+-++
             *           | | | | |              | | | | |
             *         edge connector         edge connector
             */

            for (uint8_t i = 0; i < NDOTM_NUMCOLS; i++) {
              buf[i] = buf[i] >> 1;
              buf[i] |= c & (1 << i) ? 0b01000000 : 0;
            }

              break;

            case 3:
            for (uint8_t i = 0; i < NDOTM_NUMCOLS; i++) {
              buf[i] = buf[i] << 1;
              buf[i] |= c & (1 << i) ? 0b00000001 : 0;
            }


              break;

          }

          indata_state   = indata_state_norm;
          Mode           = ModeNorm;
          buf_contents   = NDOTM_BUF_CONTENTS_BINARY;
          pin_end_is_top = true;                      // so transmitted LSB is bottom row
          break;

        case indata_state_rx_single_cmd_opcode:
          // get paramater byte that follows command opcode 
          switch(last_cmd) {
            case ndotm_cmd_transition:
              transition_ctr = transition_max = c;
              break;
            case ndotm_cmd_dwell:
              dwell_div = dwell_ctr = c;
              break;
            case ndotm_cmd_rate:
              scroll_rate_div = scroll_rate_ctr = c;
              break;
            case ndotm_cmd_font:
              if (c == 0)
                cur_font = cur_font_5x7;
              else
                cur_font = cur_font_3x5;
              break;
            case ndotm_cmd_shift_dir:
              shift_dir = c;
              break;
            default:
              break;
          }
          Mode         = ModeStartTransition;
          indata_state = indata_state_norm;
          break;

        case indata_state_rx_double_cmd_opcode:
          buf[ctr] = c;

          //NDOTM_BLIP_ON_SCOPE(10);

          if (++ctr > 1) {
            buf[ctr] = 0;
            // got both. we are done.
            cur_font     = cur_font_3x5;
            buf_contents = NDOTM_BUF_CONTENTS_2ASCII;
            Mode         = ModeStartTransition;
            indata_state = indata_state_norm;
            if (last_cmd == ndotm_cmd_2ch_flipped)
              flip2char    = true;

          }
          break;


        default:
          break;


      }

    }
  } // if (indata_available) 
  ENABLE_INDATA_IRUPS;
}

void inline NovaDotMatrix::CommonLoopChores() {
  // most of the common work done no matter what state we are in...
  if (!ATtinyTimerFiveHundredHzCtr)  {
    WriteNextCol(); // Most of the work is done in WriteNextCol()

    if (col_num_leds_on <= 3)
      // bit of a brightness leveling hack..
      // mitigate uneven brightness issues...
      // don't leave on as long since fewer leds are on..
      // otherwise they'll appear brighter due to current suck.
      // TODO: This can be obviated in the next rev of hardware by putting
      // the series resistors in the right place
      ATtinyTimerFiveHundredHzCtr = ATT_FIVE_HUNDRED_HZ_SHORTDIV;
    else
      ATtinyTimerFiveHundredHzCtr = ATtinyTimerFiveHundredHzDiv;
  }
}
uint8_t NovaDotMatrix::GetFont(uint8_t index, uint8_t offset) {
  // get character out of font table
  if (cur_font == cur_font_5x7) {
    return (pgm_read_byte( (font_5x7_2 + index * 5) + offset ));
  } else if (cur_font == cur_font_3x5) {
    if (offset < 3)
      return (pgm_read_byte( (font_3x5_1 + index * 3) + offset ) << 0);
    else
      return(0);
  }

}

void NovaDotMatrix::WriteNextCol() {
  // Called during multiplexing to write out the next column of character data
  // Implements scrolling
  char *txt_nextp, space;

  switch(Mode) {

    case ModeNorm:
      // simple case. Just write them  w/no scrolly stuff
      switch (buf_contents){
        case NDOTM_BUF_CONTENTS_ASCII:
          coldata[0] = GetFont(*txt_curp-32,0); // pgm_read_byte( (cur_fontp + (*txt_curp - 32) * 5) + 0 );
          coldata[1] = GetFont(*txt_curp-32,1); 
          coldata[2] = GetFont(*txt_curp-32,2); 
          coldata[3] = GetFont(*txt_curp-32,3); 
          coldata[4] = GetFont(*txt_curp-32,4); 
          break;

        case NDOTM_BUF_CONTENTS_BINARY:
          coldata[0] = *(txt_curp + 0);
          coldata[1] = *(txt_curp + 1);
          coldata[2] = *(txt_curp + 2);
          coldata[3] = *(txt_curp + 3);
          coldata[4] = *(txt_curp + 4);
          break;

        case NDOTM_BUF_CONTENTS_2ASCII:
          DispTwoSmallChars(flip2char);
          break;

        default:
          break;
      }
      NDOTM_WRITE_AND_UPDATE_COL_COUNTER;
      break;

    case ModeStartTransition:
      break;

    case ModeInTransition:

      if (transition_max != 0)
        // simple case. Just write them  w/no scrolly stuff
        coldata[0] = coldata[1] = coldata[2] = coldata[3] = coldata[4] =  0b00000000; // blank during transition

      NDOTM_WRITE_AND_UPDATE_COL_COUNTER;
      break;

    case ModeStartScrollMessage:
      coldata[0] = coldata[1] = coldata[2] = coldata[3] = coldata[4] =  0b00000000; 
      Mode = ModeScrollMessage;
      NDOTM_WRITE_AND_UPDATE_COL_COUNTER;
      break;

    case ModeScrollMessage:
      space = ' ';
      txt_nextp = txt_curp + 1;
      if (!(*txt_nextp))
        txt_nextp = &space;

      if (lastscrollstep != scrollstep) {

        switch (scrollstep) {
          case 0:
            coldata[0] = GetFont((*txt_curp-32), 0); 
            coldata[1] = GetFont((*txt_curp-32), 1); 
            coldata[2] = GetFont((*txt_curp-32), 2);
            coldata[3] = GetFont((*txt_curp-32), 3);
            coldata[4] = GetFont((*txt_curp-32), 4);

            dwell_ctr = dwell_div;
            break;

          case 1:
            // shift off left
            coldata[0] = coldata[1];
            coldata[1] = coldata[2];
            coldata[2] = coldata[3];
            coldata[3] = coldata[4];
            // space
            coldata[4] = 0; 
            break;


          case 2:
            // shift off left going..
            coldata[0] = coldata[1];
            coldata[1] = coldata[2];
            coldata[2] = coldata[3];
            coldata[3] = 0;
            coldata[4] = 0; 
            break;

          case 3:
            // shift off left going..
            coldata[0] = coldata[1];
            coldata[1] = coldata[2];
            coldata[2] = 0;
            coldata[3] = 0;
            // next character appears
            coldata[4] = GetFont((*txt_curp-32), 0);

            break;

          case 4:
            coldata[0] = coldata[1];
            coldata[1] = 0;
            coldata[2] = 0;
            coldata[3] = coldata[4];
            coldata[4] = GetFont((*txt_curp-32), 1);

            break;

          case 5:
            coldata[0] = 0;
            coldata[1] = 0;
            coldata[2] = coldata[3];
            coldata[3] = coldata[4];
            coldata[4] = GetFont((*txt_curp-32), 2);
            break;

          case 6:
            coldata[0] = 0;
            coldata[1] = coldata[2];
            coldata[2] = coldata[3];
            coldata[3] = coldata[4];
            coldata[4] = GetFont((*txt_curp-32), 3);
            break;

          case 7:
            coldata[0] = coldata[1];
            coldata[1] = coldata[2];
            coldata[2] = coldata[3];
            coldata[3] = coldata[4];
            coldata[4] = GetFont((*txt_curp-32), 4);

            //NDOTM_BLIP_ON_SCOPE(10);
            txt_curp++;
            if (!(*txt_curp))
              txt_curp = txt_headp;

            scrollstep = 1;
            dwell_ctr = dwell_div;
            break;

          default:
            break; 

        } // switch (scrollstep) 

      } //  if (lastscrollstep != scrollstep) 

      lastscrollstep = scrollstep;

      NDOTM_WRITE_AND_UPDATE_COL_COUNTER;

      break;
    default:
      break;
  } // switch(Mode)

}

void NovaDotMatrix::WriteCol(uint8_t colno,uint8_t rowdat) {
  //
  // load display colno with data in rowdat
  //
  uint8_t mask,colbit;

  if (pin_end_is_top) {
    mask   = 0b00010000;
    colbit = 0b00000001 << (4-colno);
  } else {

    mask   = 0b00001000;
    colbit = 0b00000001 << (7-colno);
  }

  // column
  uint8_t i;

#if !defined(NDOTM_TESTING)
  PORTB |= NDOTM_BLANK_DATOUT_BIT; // disable all columns by shutting off their current
#endif

  //
  // column bits first
  //
  for (i = 0;i < 5; i++) {

    // set data bit to 0 or 1
    if (mask & colbit) 
      PORTB &= ~NDOTM_SR_DAT_BIT; 
    else  
      PORTB |= NDOTM_SR_DAT_BIT; 

    // toggle clock
    PORTB &= ~NDOTM_SR_CLK_BIT; 
    PORTB |= NDOTM_SR_CLK_BIT; 

    if (pin_end_is_top)
      mask = mask >> 1;
    else
      mask = mask << 1;


  }
  // extra clock for unused bit..
  PORTB &= ~NDOTM_SR_CLK_BIT; 
  PORTB |= NDOTM_SR_CLK_BIT; 

  //
  // now row bits
  //
  if (pin_end_is_top)
    mask = 0b01000000;
  else
    mask = 0b00000001;

  col_num_leds_on = 0;
  for (i = 0;i < 7; i++) {

    if (mask & rowdat)  {
      PORTB &= ~NDOTM_SR_DAT_BIT; 
      col_num_leds_on++;
    }
    else  {
      PORTB |= NDOTM_SR_DAT_BIT; 
    }

    PORTB &= ~NDOTM_SR_CLK_BIT;  
    PORTB |= NDOTM_SR_CLK_BIT; 

    if (pin_end_is_top)
      mask = mask >> 1;
    else
      mask = mask << 1;
  }
#if !defined(NDOTM_TESTING)
  PORTB &= ~NDOTM_BLANK_DATOUT_BIT; // enable column drivers
#endif

}


void NovaDotMatrix::ScrollAndDwellManage(void) {
  //
  // Scroll rate and Dwell period  
  //
  if (!(attinytimer.OneHundredHzFlag))
    return;

  attinytimer.OneHundredHzFlag = false;

  if (dwell_ctr) {
    dwell_ctr--;
  }

  if (!dwell_ctr && !(--scroll_rate_ctr)) { 
    scrollstep++;
    if (scrollstep > 7)
      scrollstep = 0;

    scroll_rate_ctr = scroll_rate_div;
  }

  if (Mode == ModeInTransition) {
    // transition is the period between display of data
    // display will be showing transition effects

    if (transition_ctr++ > transition_max) {
      // when done, switch display mode back to norm
      txt_curp = (char *)buf;
      Mode = ModeNorm;
    }
  }

  /*
  */

}
void NovaDotMatrix::DispTwoSmallChars(bool flip2char) {
  // rotate character 90 degrees

  /* DispTwoSmallChars 
   *
   *  Bits to move:
   *             source                 dest
   *           coldata[]              coldata[]
   *    bitpos  v v v v v      bitpos  v v v v v
   *        v  |0|1|2|3|4|         v  |0|1|2|3|4|
   *       [0]:|a|b|c| | |        [0]:| | | | | |
   *       [1]:|d|e|f| | |        [1]:| | | | | |
   *       [2]:|g|h|i| | |  --->  [2]:| | | | | |
   *       [3]:|j|k|l| | |        [3]:| | | | | |
   *       [4]:|m|n|o| | |        [4]:|c|f|i|l|o|
   *       [5]:| | | | | |        [5]:|b|e|h|k|n|
   *       [6]:| | | | | |        [6]:|a|d|g|j|m|
   *          +++++++++++            ++-+-+-+-++
   *          | | | | |              | | | | |
   *        edge connector         edge connector
   *
   *         --source-- --dest----
   *         col : bit : col : bit
   *         a 0   : 0   :  0  :  6
   *         b 1   : 0   :  0  :  5
   *         c 2   : 0   :  0  :  4
   *
   *         d 0   : 1   :  1  :  6
   *         e 1   : 1   :  1  :  5
   *         f 2   : 1   :  1  :  4
   *
   *         g 0   : 2   :  2  :  6
   *         h 1   : 2   :  2  :  5
   *         i 2   : 2   :  2  :  4
   *
   *         j 0   : 3   :  3  :  6
   *         k 1   : 3   :  3  :  5
   *         l 2   : 3   :  3  :  4
   *
   *         m 0   : 4   :  4  :  6
   *         n 1   : 4   :  4  :  5
   *         n 2   : 4   :  4  :  4
   *
   */

  uint8_t tmp[3];
  unsigned char c;

  if (flip2char) {
    // display upside down

    coldata[0] = 0;
    coldata[1] = 0;
    coldata[2] = 0;
    coldata[3] = 0;
    coldata[4] = 0;


    c = (*txt_curp)-32;

    tmp[0] = GetFont(c,0); 
    tmp[1] = GetFont(c,1); 
    tmp[2] = GetFont(c,2); 

    // TODO: this could be rolled up 

    // pins on left

    //                    76543210      76543210
    coldata[0] |= (tmp[0] & 0b00000001) ? 0b01000000 : 0;
    coldata[0] |= (tmp[1] & 0b00000001) ? 0b00100000 : 0;
    coldata[0] |= (tmp[2] & 0b00000001) ? 0b00010000 : 0;
    //                    76543210      76543210
    coldata[1] |= (tmp[0] & 0b00000010) ? 0b01000000 : 0;
    coldata[1] |= (tmp[1] & 0b00000010) ? 0b00100000 : 0;
    coldata[1] |= (tmp[2] & 0b00000010) ? 0b00010000 : 0;
    //                    76543210      76543210
    coldata[2] |= (tmp[0] & 0b00000100) ? 0b01000000 : 0;
    coldata[2] |= (tmp[1] & 0b00000100) ? 0b00100000 : 0;
    coldata[2] |= (tmp[2] & 0b00000100) ? 0b00010000 : 0;
    //                    76543210      76543210
    coldata[3] |= (tmp[0] & 0b00001000) ? 0b01000000 : 0;
    coldata[3] |= (tmp[1] & 0b00001000) ? 0b00100000 : 0;
    coldata[3] |= (tmp[2] & 0b00001000) ? 0b00010000 : 0;
    //                    76543210      76543210
    coldata[4] |= (tmp[0] & 0b00010000) ? 0b01000000 : 0;
    coldata[4] |= (tmp[1] & 0b00010000) ? 0b00100000 : 0;
    coldata[4] |= (tmp[2] & 0b00010000) ? 0b00010000 : 0;
    /*

       source                 dest
       coldata[]              coldata[]
       bitpos  v v v v v      bitpos  v v v v v
       v  |0|1|2|3|4|         v  |0|1|2|3|4|
       [0]:|a|b|c| | |        [0]:|c|f|i|l|o|
       [1]:|d|e|f| | |        [1]:|b|e|h|k|n|
       [2]:|g|h|i| | |  --->  [2]:|a|d|g|j|m|
       [3]:|j|k|l| | |        [3]:| | | | | |
       [4]:|m|n|o| | |        [4]:| | | | | |
       [5]:| | | | | |        [5]:| | | | | |
       [6]:| | | | | |        [6]:| | | | | |
       +++++++++++            ++-+-+-+-++
       | | | | |              | | | | |
       edge connector         edge connector

       --source-- --dest----
col : bit : col : bit
a 0   : 0   :  0  :  2
b 1   : 0   :  0  :  1
c 2   : 0   :  0  :  0

d 0   : 1   :  1  :  2
e 1   : 1   :  1  :  1
f 2   : 1   :  1  :  0

g 0   : 2   :  2  :  2
h 1   : 2   :  2  :  1
i 2   : 2   :  2  :  0

j 0   : 3   :  3  :  2
k 1   : 3   :  3  :  1
l 2   : 3   :  3  :  0

m 0   : 4   :  4  :  2
n 1   : 4   :  4  :  1
n 2   : 4   :  4  :  0
*/

    c = (*(txt_curp+1))-32;

    tmp[0] = GetFont(c,0);
    tmp[1] = GetFont(c,1);
    tmp[2] = GetFont(c,2);

    //                    76543210      76543210
    coldata[0] |= (tmp[0] & 0b00000001) ? 0b00000100 : 0;
    coldata[0] |= (tmp[1] & 0b00000001) ? 0b00000010 : 0;
    coldata[0] |= (tmp[2] & 0b00000001) ? 0b00000001 : 0;
    //                    76543210      76543210
    coldata[1] |= (tmp[0] & 0b00000010) ? 0b00000100 : 0;
    coldata[1] |= (tmp[1] & 0b00000010) ? 0b00000010 : 0;
    coldata[1] |= (tmp[2] & 0b00000010) ? 0b00000001 : 0;
    //                    76543210      76543210
    coldata[2] |= (tmp[0] & 0b00000100) ? 0b00000100 : 0;
    coldata[2] |= (tmp[1] & 0b00000100) ? 0b00000010 : 0;
    coldata[2] |= (tmp[2] & 0b00000100) ? 0b00000001 : 0;
    //                    76543210      76543210
    coldata[3] |= (tmp[0] & 0b00001000) ? 0b00000100 : 0;
    coldata[3] |= (tmp[1] & 0b00001000) ? 0b00000010 : 0;
    coldata[3] |= (tmp[2] & 0b00001000) ? 0b00000001 : 0;
    //                    76543210      76543210
    coldata[4] |= (tmp[0] & 0b00010000) ? 0b00000100 : 0;
    coldata[4] |= (tmp[1] & 0b00010000) ? 0b00000010 : 0;
    coldata[4] |= (tmp[2] & 0b00010000) ? 0b00000001 : 0;

  } else  {
    // pins on right

    /*
     *            source                 dest
     *           coldata[]              coldata[]
     *   bitpos  v v v v v      bitpos  v v v v v
     *       v  |0|1|2|3|4|         v  |0|1|2|3|4|
     *      [0]:|a|b|c| | |        [0]:|m|j|g|d|a|
     *      [1]:|d|e|f| | |        [1]:|n|k|h|e|b|
     *      [2]:|g|h|i| | |  --->  [2]:|o|l|i|f|c|
     *      [3]:|j|k|l| | |        [3]:| | | | | |
     *      [4]:|m|n|o| | |        [4]:| | | | | |
     *      [5]:| | | | | |        [5]:| | | | | |
     *      [6]:| | | | | |        [6]:| | | | | |
     *          +++++++++++            ++-+-+-+-++
     *           | | | | |              | | | | |
     *         edge connector         edge connector
     *
     * 
     */

    coldata[0] = 0;
    coldata[1] = 0;
    coldata[2] = 0;
    coldata[3] = 0;
    coldata[4] = 0;


    c = (*txt_curp)-32;

    tmp[0] = GetFont(c,0); 
    tmp[1] = GetFont(c,1); 
    tmp[2] = GetFont(c,2); 

    // pins on left

    //                        76543210      76543210
    coldata[4] |= (tmp[0] & 0b00000001) ? 0b00000001 : 0; // a
    coldata[4] |= (tmp[1] & 0b00000001) ? 0b00000010 : 0; // b
    coldata[4] |= (tmp[2] & 0b00000001) ? 0b00000100 : 0; // c
    //                        76543210      76543210
    coldata[3] |= (tmp[0] & 0b00000010) ? 0b00000001 : 0; // d
    coldata[3] |= (tmp[1] & 0b00000010) ? 0b00000010 : 0; // e
    coldata[3] |= (tmp[2] & 0b00000010) ? 0b00000100 : 0; // f
    //                        76543210      76543210
    coldata[2] |= (tmp[0] & 0b00000100) ? 0b00000001 : 0; // g
    coldata[2] |= (tmp[1] & 0b00000100) ? 0b00000010 : 0; // h
    coldata[2] |= (tmp[2] & 0b00000100) ? 0b00000100 : 0; // i
    //                        76543210      76543210
    coldata[1] |= (tmp[0] & 0b00001000) ? 0b00000001 : 0; // j
    coldata[1] |= (tmp[1] & 0b00001000) ? 0b00000010 : 0; // k
    coldata[1] |= (tmp[2] & 0b00001000) ? 0b00000100 : 0; // l
    //                        76543210      76543210
    coldata[0] |= (tmp[0] & 0b00010000) ? 0b00000001 : 0; // m
    coldata[0] |= (tmp[1] & 0b00010000) ? 0b00000010 : 0; // n
    coldata[0] |= (tmp[2] & 0b00010000) ? 0b00000100 : 0; // o

    /*
     *            source                 dest
     *           coldata[]              coldata[]
     *   bitpos  v v v v v      bitpos  v v v v v
     *       v  |0|1|2|3|4|         v  |0|1|2|3|4|
     *      [0]:|a|b|c| | |        [0]:| | | | | |
     *      [1]:|d|e|f| | |        [1]:| | | | | |
     *      [2]:|g|h|i| | |  --->  [2]:| | | | | |
     *      [3]:|j|k|l| | |        [3]:| | | | | |
     *      [4]:|m|n|o| | |        [4]:|m|j|g|d|a|
     *      [5]:| | | | | |        [5]:|n|k|h|e|b|
     *      [6]:| | | | | |        [6]:|o|l|i|f|c|
     *          +++++++++++            ++-+-+-+-++
     *           | | | | |              | | | | |
     *         edge connector         edge connector
     *
     * 
     */
    c = (*(txt_curp+1))-32;

    tmp[0] = GetFont(c,0);
    tmp[1] = GetFont(c,1);
    tmp[2] = GetFont(c,2);
    //                        76543210      76543210
    coldata[4] |= (tmp[0] & 0b00000001) ? 0b00010000 : 0; // a
    coldata[4] |= (tmp[1] & 0b00000001) ? 0b00100000 : 0; // b
    coldata[4] |= (tmp[2] & 0b00000001) ? 0b01000000 : 0; // c
    //                        76543210      76543210
    coldata[3] |= (tmp[0] & 0b00000010) ? 0b00010000 : 0; // d
    coldata[3] |= (tmp[1] & 0b00000010) ? 0b00100000 : 0; // e
    coldata[3] |= (tmp[2] & 0b00000010) ? 0b01000000 : 0; // f
    //                        76543210      76543210
    coldata[2] |= (tmp[0] & 0b00000100) ? 0b00010000 : 0; // g
    coldata[2] |= (tmp[1] & 0b00000100) ? 0b00100000 : 0; // h
    coldata[2] |= (tmp[2] & 0b00000100) ? 0b01000000 : 0; // i
    //                        76543210      76543210
    coldata[1] |= (tmp[0] & 0b00001000) ? 0b00010000 : 0; // j
    coldata[1] |= (tmp[1] & 0b00001000) ? 0b00100000 : 0; // k
    coldata[1] |= (tmp[2] & 0b00001000) ? 0b01000000 : 0; // l
    //                        76543210      76543210
    coldata[0] |= (tmp[0] & 0b00010000) ? 0b00010000 : 0; // m
    coldata[0] |= (tmp[1] & 0b00010000) ? 0b00100000 : 0; // n
    coldata[0] |= (tmp[2] & 0b00010000) ? 0b01000000 : 0; // o

  }

  return;


}

#ifdef NDOTM_COMPILE_DEMO

static uint8_t GetRandomGraph(void) {
  // returns a variable byte with variable # of bits set
  // call repeatedly to form a graph
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

static bool did_once               = false;
#define DEMO_STEP_INTERVAL 10
static uint8_t demo_step_ctr       = DEMO_STEP_INTERVAL;
static int8_t inner_demo_ctr       = 0; // inner counter for timing modulations within a demo
static int8_t inner_demo_step      = 1; // they are signed so we can add/subtract

#define DEMO_SWITCH_INTERVAL 255
static uint8_t demo_switch_ctr     = DEMO_SWITCH_INTERVAL;
static uint8_t innermost_demo_ctr       = 0; // finest grain demo modulation

static void RandomCrawlingGraph(uint8_t dir) { 
  /* random crawling graph */ 
  if (!did_once) { 
    WRITE_SELF(ndotm_cmd_escape_code); 
    WRITE_SELF(ndotm_cmd_shift_dir); 
    WRITE_SELF(dir); /* shift direction */ 

    WRITE_SELF(ndotm_cmd_escape_code);
    WRITE_SELF(ndotm_cmd_transition);
    WRITE_SELF(0);
    did_once = true; 
  } 
  if (!(--demo_step_ctr)) { 
    WRITE_SELF(ndotm_cmd_escape_code); 
    WRITE_SELF(ndotm_cmd_data_scroll); 
    WRITE_SELF(GetRandomGraph()); 
    demo_step_ctr = 2; 
  } 
}
uint8_t myrandom(void) {
  static uint16_t adr = 0x1000;
  if (adr++ > 4096)
    adr = 0x40;
  return pgm_read_byte(adr); 

}

static void RandomCrawl(uint8_t dir) { 
  uint8_t c;

  if (!did_once) { 
    inner_demo_ctr = inner_demo_step = 1; 
    did_once       = true; 
    WRITE_SELF(ndotm_cmd_escape_code); 
    WRITE_SELF(ndotm_cmd_shift_dir); 
    WRITE_SELF(dir); 

    WRITE_SELF(ndotm_cmd_escape_code);
    WRITE_SELF(ndotm_cmd_transition);
    WRITE_SELF(0);

  } 

  if (!(--demo_step_ctr)) { 
    demo_step_ctr = 2; 

    WRITE_SELF(ndotm_cmd_escape_code); 
    WRITE_SELF(ndotm_cmd_data_scroll); 

    c = 0; 
    for (uint8_t i = 0; i < NDOTM_NUMROWS; i++) { 
      c |= ((myrandom()>>1) > 100) ? (1 << i) : 0; 
    } 
    WRITE_SELF(c); 

    inner_demo_ctr += inner_demo_step; 
    if (inner_demo_ctr <= 1 || inner_demo_ctr >= 126) 
      inner_demo_step = -inner_demo_step; 

  } 
}


static void DemoManage() {
  //
  // sequencing of demos done here.
  //
#define START_DEMO 0
#define END_DEMO 17
#define MAX_DEMO 17  // highest #'d demo

  static uint8_t demo_number         = START_DEMO;

  static uint8_t c                   = '0';
  uint8_t i;


#define BUFSIZE 32
  static char demo_buf[BUFSIZE];
#define SCROLL_TEXT_MESSAGE "01234567890ABCDEF" //  }; // = { "WELCOME TO NOVA LABS..." };

if (!(attinytimer.FiftyHzFlag)) // everything gets done at 50hz intervals
  return;
  attinytimer.FiftyHzFlag = false;


#if 0
  static uint8_t subdiv = 10;
if (--subdiv)
  return;

  subdiv = 10;
#endif


#ifdef ONE_DEMO
  demo_number = ONE_DEMO;
#endif

  switch(demo_number) {

    case 0: 
      // ---------------------
      // sequential characters
      //
      if (!did_once) {
        did_once = true;
        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_reset);
      }
      if (!(--demo_step_ctr)) {
        WRITE_SELF(c);
        if (c++ == '[')
          // next sequential character
          c = 'A';

        demo_step_ctr = DEMO_STEP_INTERVAL;
#if 0
        inner_demo_ctr += inner_demo_step;
        if (inner_demo_ctr <= 0 || inner_demo_ctr >= 7)
          // if out of range toggle direction..
          inner_demo_step = -inner_demo_step;

        // we use inner_demo_ctr to modulate transition period
        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_transition);
        WRITE_SELF(inner_demo_ctr);
#endif

      }
      break;

    case 1:
      // ---------------------
      // message scroll
      if (!did_once) {
        // message
        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_message);
        strcpy(demo_buf,SCROLL_TEXT_MESSAGE);
        char *cp = demo_buf;
        for (i = 0; i < strlen(demo_buf) + 1; i++) {
          // write message plus null
          WRITE_SELF(*cp++);
        }
        did_once = true;
      }
      break;

    case 2: 
#if 0 // boring
      // ---------------------
      // sequential alternate font
      if (!did_once) {
        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_font);
        WRITE_SELF(1); // switch to alternate font

        did_once = true;
      }

      if (!(--demo_step_ctr)) {
        // fake out input logic that char is available
        WRITE_SELF(c);
        if (c++ == '[')
          c = 'A';

        demo_step_ctr = DEMO_STEP_INTERVAL;

      }
#else
      demo_number++;
#endif
      break;

    case 3: 
      // ---------------------
      // wiggly triangle wave  scroll left
      if (!(--demo_step_ctr)) {

        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_data_scroll);
        WRITE_SELF(1 << inner_demo_ctr);

        inner_demo_ctr += inner_demo_step;
        if (inner_demo_ctr < 0 || inner_demo_ctr > 7)
          inner_demo_step = -inner_demo_step; // if out of range toggle direction..

        demo_step_ctr = 5;
      }

      break;

    case 4: 
      // ---------------------
      // random scroll left
      if (!did_once) {
        randomSeed(0xAA);
        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_reset);
        demo_step_ctr = 1;
        inner_demo_ctr       = 1;
        inner_demo_step      = 1;
        did_once = true;
      }

      if (!(--demo_step_ctr)) {

        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_data_scroll);
        WRITE_SELF(random(128)); // WRITE_SELF(1 << inner_demo_ctr);
#if 0
        inner_demo_ctr += inner_demo_step;
        if (inner_demo_ctr <= 1 || inner_demo_ctr >= 10)
          inner_demo_step = -inner_demo_step; // if out of range toggle direction..

        demo_step_ctr += inner_demo_ctr; // modulate rate with inner_demo_ctr;
#else
        demo_step_ctr = 2;
#endif

      }

      break;

    case 5:
      // ---------------------
      // random crawling graph
      if (!(--demo_step_ctr)) {
        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_data_scroll);
        WRITE_SELF(GetRandomGraph());

        demo_step_ctr = 2;

      }

      break;

    case 6:
      // ---------------------
      // random bits drifting left and right  
      if (!did_once) {
        // first time through fill demo_buf with random bits
        // random
        randomSeed(random(128));
        for (i = 0; i < NDOTM_NUMCOLS; i++) 
          demo_buf[i] = random(128);

        innermost_demo_ctr = 3;
        inner_demo_ctr = 3;
        demo_step_ctr = 3;

        did_once = true;
      }
      if (!(--demo_step_ctr)) {

        switch(innermost_demo_ctr) {

          case 0:
            SHIFT_RIGHT_WRAP(demo_buf);
            break;

          case 1:
            SHIFT_LEFT_WRAP(demo_buf); 
            break;

          case 2:
            SHIFT_UP_WRAP(demo_buf);
            break;

          case 3:
            SHIFT_DOWN_WRAP(demo_buf);
            break;

          default:
            break;

        }

        // outermost counter controls basic step rate
        // through animation only every N'th time through..
        demo_step_ctr = 3;

        if (!(--inner_demo_ctr)) {
          inner_demo_ctr = 15;

          if (!(innermost_demo_ctr--))
            innermost_demo_ctr = 3;

        }

        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_data);
        for (i = 0; i < NDOTM_NUMCOLS; i++)  {
          WRITE_SELF(demo_buf[i]);
        }

      }
      break;

    case 7:
      // fill with pattern
      if (!did_once) {
        did_once = true;
        demo_buf[0] = 0b00011100; 
        demo_buf[1] = 0b00111000; 
        demo_buf[2] = 0b01110000; 
        demo_buf[3] = 0b00111000; 
        demo_buf[4] = 0b00011100; 
      }

      if (!(--demo_step_ctr)) {

        demo_step_ctr = 4;
        SHIFT_UP_WRAP(demo_buf);

        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_data);
        for (i = 0; i < NDOTM_NUMCOLS; i++)  {
          WRITE_SELF(demo_buf[i]);
        }

      }
      break;

    case 8:
      // -----------------------------------
      // show two small digits incrementing landscape
      if (!did_once) {
        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_transition);
        WRITE_SELF(0);
        did_once       = true;
        inner_demo_ctr = 0;
      }

      if (!(--demo_step_ctr)) {
        demo_step_ctr = 10;

        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_2ch_flipped);

        if (inner_demo_ctr++ > 99)
          inner_demo_ctr = 0;

        // tens digit
        c = (inner_demo_ctr/10) + '0'; // turn it into ascii
        WRITE_SELF(c);

        // ones digit
        c = (inner_demo_ctr%10) + '0'; // turn it into ascii
        WRITE_SELF(c);

      }
      break;

    case 9:
      // -----------------------------------
      // show two small digits incrementing landscape
      if (!did_once) {
        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_transition);
        WRITE_SELF(0);
        did_once       = true;
        inner_demo_ctr = 0;
      }

      if (!(--demo_step_ctr)) {
        demo_step_ctr = 3;

        WRITE_SELF(ndotm_cmd_escape_code);
        WRITE_SELF(ndotm_cmd_2ch);

        if (inner_demo_ctr++ > 99)
          inner_demo_ctr = 0;

        // tens digit
        c = (inner_demo_ctr/10) + '0'; // turn it into ascii
        WRITE_SELF(c);

        // ones digit
        c = (inner_demo_ctr%10) + '0'; // turn it into ascii
        WRITE_SELF(c);

      }
      break;

    case 10:
      RandomCrawlingGraph(0);
      break;

    case 11:
      RandomCrawlingGraph(1);
      break;

    case 12:
      RandomCrawlingGraph(2);
      break;

    case 13:
      RandomCrawlingGraph(3);
      break;

    case 14:
      RandomCrawl(0);
      break;

    case 15:
      RandomCrawl(1);
      break;
    case 16:
      RandomCrawl(2);
      break;
    case MAX_DEMO:
      RandomCrawl(3);
      break;
#if 0
#endif


    default:
      break;
  }


// -----------------------
// each time through the demo tom-foolery we check to see if it is time to go on to the next

if (!(--demo_switch_ctr)) {
  // hard-code to do just one demo

  if (++demo_number > END_DEMO)
    demo_number = START_DEMO;

  demo_switch_ctr = DEMO_SWITCH_INTERVAL;
  did_once = false;

  WRITE_SELF(ndotm_cmd_escape_code);
  WRITE_SELF(ndotm_cmd_reset);
}

}

#endif // NDOTM_COMPILE_DEMO



ISR(PCINT0_vect) 
{
  // 
  // interrupts from external master clock line come here
  // 
  bool clk_in_high = *portInputRegister(novadotmatrix.indata_port) & NDOTM_CLK_IN_BIT;
  static uint8_t indata_raw;

  if (novadotmatrix.demo)
    return;

  if (novadotmatrix.indata_available  || !clk_in_high) {
    // we've already told them upstairs so nothing to do until
    // they clear indata_available
    //NDOTM_BLIP_ON_SCOPE(1);
    return;
  }
  novadotmatrix.indata_idle_ctr = 0;
  if (clk_in_high) {

    /* !novadotmatrix.indata_available && <- already tested */ // ignore next incoming if top loop hasn't grabbed data (this should be an error)
    // it is actually an interrupt on change and we are only interested in when the clock level is high
    // *portInputRegister(novadotmatrix.indata_port) & NDOTM_CLK_IN_BIT) { // (don't know why PORTB & NDOTM_CLK_IN_BIT doesn't work)
    // if clock is high 
    if (*portInputRegister(novadotmatrix.indata_port) & NDOTM_DAT_IN_BIT)  {
      // then if data is high
      //novadotmatrix.indata |= (1 << novadotmatrix.indata_cur_bit); // set next received bit 
      indata_raw |= (1 << novadotmatrix.indata_cur_bit); // set next received bit 
    }
    if (!novadotmatrix.indata_cur_bit) {
      novadotmatrix.indata = indata_raw;
      indata_raw = 0;
      novadotmatrix.indata_available = true;
      novadotmatrix.indata_cur_bit = 7;
    } else {
      novadotmatrix.indata_cur_bit--;
    }

    novadotmatrix.indata_idle_ctr = 0;
  }

  }
