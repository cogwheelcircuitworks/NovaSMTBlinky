/*
  NovaDotMatrix Library Functions

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

*/

#ifndef NovaDotMatrix_h
#define NovaDotMatrix_h
#include "Arduino.h"
#include "ATTinyTimer.h"

//#define NDOTM_TESTING // mostly for scope blip borrows blanking pin

#define NDOTM_DEMO_TEN_HZ_BIT       0b00000001       // our bit in the timer's flag byte
#define NDOTM_MANAGE_HUNDRED_HZ_BIT 0b00000001 // our bit in the timer's flag byte

// Pin Assignments
// display control
#define NDOTM_COL_ENABLE_PIN   PB1         // column enable
#define NDOTM_COL_ENABLE_BIT   0b00000010

#define NDOTM_BLANK_DATOUT_PIN PB1
#define NDOTM_BLANK_DATOUT_BIT 0b00000010

#define NDOTM_SR_CLK_PIN       PB3         // shift register clock
#define NDOTM_SR_CLK_BIT       0b00001000

#define NDOTM_SR_DAT_PIN       PB4         // shift register data
#define NDOTM_SR_DAT_BIT       0b00010000

#define NDOTM_DAT_OUT_PIN      PB1         // data out to host
#define NDOTM_DAT_OUT_BIT      0b00000010
                                           // host interface

#define NDOTM_CLK_IN_PIN       PB2         // clock from host
#define NDOTM_CLK_IN_BIT       0b00000100

#define NDOTM_DAT_IN_PIN       PB0         // data from host
#define NDOTM_DAT_IN_BIT       0b00000001

class NovaDotMatrix
{
  public:
    void Setup(void);  // call once
    void Loop(void); // call continuously
    inline void CommonLoopChores(void); 
    uint8_t Mode;
    uint8_t NextMode;
    enum mode { 
      // Loop(mode) where mode =
      ModeReset = 1,
      ModeNorm,
      ModeRxData, // receiving data
      ModeStartScrollMessage, // scrolling message
      ModeScrollMessage, // scrolling message
      ModeStartTransition, // start display transition
      ModeInTransition,
    };
    // communications from master
    volatile uint8_t indata;
    volatile bool indata_available;
    volatile uint8_t indata_cur_bit;
    volatile uint8_t indata_idle_ctr;
    const uint8_t indata_idle_max = 2;
    const uint8_t indata_fast_ctr_flag_bit = 0b00000001;

    uint8_t shift_dir;

    uint8_t indata_state;
    enum indata_state {
      indata_state_norm_buf_ascii,
      indata_state_norm_buf_raw,
      indata_state_norm,
      indata_state_rx_single_cmd_opcode,
      indata_state_rx_double_cmd_opcode,
      indata_state_rx_data,
      indata_state_rx_data_single_byte_for_scroll,
      indata_state_rx_message
    };
    uint8_t indata_port;
    void ProcessInData(void);
    bool demo;

    uint8_t cur_font;
    enum cur_font {
    cur_font_5x7 = 1,
    cur_font_3x5
    };


    uint8_t transition_ctr; // period we stay
    uint8_t transition_max; // counts up 
#define NDOTM_TRANSITION_MAX 2

private:
    void Chores(void);
    void ScrollAndDwellManage(void);
    void WriteCol(uint8_t, uint8_t); 
    void WriteNextCol(void);
    void DispTwoSmallChars(bool);

    uint8_t scroll_rate_ctr;
    uint8_t scroll_rate_div;
#define NDOTM_SCROLLRATE_VAL 2

    uint8_t dwell_div; // initialize to this
    uint8_t dwell_ctr; // then count down
#define NDOTM_DWELL_VAL 30


    uint8_t por_ctr ; // power-on reset counter
#define NDOTM_POR_CTR_MAX 50


    uint8_t GetFont(uint8_t,uint8_t);
    char *txt_headp, *txt_curp;

    

    uint8_t col_ctr;
    uint8_t col_num_leds_on;
#define NDOTM_NUMROWS 7 
#define NDOTM_NUMCOLS 5
    uint8_t coldata[NDOTM_NUMCOLS];
    uint8_t scrollstep;
    uint8_t lastscrollstep;
    bool pin_end_is_top;
    bool flip2char;

#define NDOTM_BUFLEN 64
    uint8_t buf[NDOTM_BUFLEN];
    uint8_t buf_contents;
#define NDOTM_BUF_CONTENTS_ASCII 0
#define NDOTM_BUF_CONTENTS_2ASCII 1
#define NDOTM_BUF_CONTENTS_BINARY 2

}; 

#define ENABLE_INDATA_IRUPS  GIMSK  |=  0b00100000;
#define DISABLE_INDATA_IRUPS  GIMSK &= ~0b00100000;

#define NDOTM_NOPS \
  __asm__ __volatile__ (\
      "nop\n" \
      "nop\n" \
      "nop\n" \
      "nop\n" \
      "nop\n" \
      "nop\n" \
      "nop\n" \
      "nop\n" \
      "nop\n" \
      )


#ifdef NDOTM_TESTING
#define NDOTM_BLIP_ON_SCOPE(x) \
{ \
  /* show pulse on scope */ \
DISABLE_TIMER_IRUPS; \
DISABLE_INDATA_IRUPS; \
for(uint8_t _i = 0;_i < x; _i++) { \
  PORTB |= NDOTM_DAT_OUT_BIT; \
  NDOTM_NOPS; \
  PORTB &= ~NDOTM_DAT_OUT_BIT; \
  NDOTM_NOPS; \
} \
ENABLE_INDATA_IRUPS; \
ENABLE_TIMER_IRUPS; \
} 
#else
#define NDOTM_BLIP_ON_SCOPE 
#endif

#define NDOTM_WRITE_AND_UPDATE_COL_COUNTER \
{ \
  /* basic act of multiplex; write one column at at a time */ \
  WriteCol(col_ctr,coldata[col_ctr]); \
  col_ctr++; \
  if (col_ctr > 4) \
  col_ctr = 0; \
} \

/* Display operations */

#define SHIFT_LEFT_WRAP(b) { \
  uint8_t tmp =  b[0]; \
  b[0] = b[1]; \
  b[1] = b[2]; \
  b[2] = b[3]; \
  b[3] = b[4]; \
  b[4] = tmp; \
}

#define SHIFT_RIGHT_WRAP(b) { \
  uint8_t tmp =  b[4]; \
  b[4] = b[3]; \
  b[3] = b[2]; \
  b[2] = b[1]; \
  b[1] = b[0]; \
  b[0] = tmp; \
}

#define SHIFT_UP_WRAP(b) { \
  uint8_t btmp,i; \
  for (i = 0; i < NDOTM_NUMCOLS; i++)  { \
    btmp = b[i] & 0b01000000; /* save msb */ \
    b[i] = (b[i] << 1) | /* shift up */ \
    (btmp >> 6); /* with msb in lsb */ \
  } \
}


#define SHIFT_DOWN_WRAP(b) { \
  uint8_t btmp,i; \
  for (i = 0; i < NDOTM_NUMCOLS; i++)  { \
    btmp = b[i] & 0b00000001; /* save lsb */ \
    b[i] = (b[i] >> 1) | /* shift down */ \
    (btmp << 6); /* replace msb with lsb */ \
  } \
}

#define WRITE_SELF(c) { /* fake sending data to ourselves for demo */ \
  novadotmatrix.indata = c; \
  novadotmatrix.indata_available = true; \
  novadotmatrix.ProcessInData(); \
}



#endif // NovaDotMatrix_h
