/*
  SMTBlinky Library Functions

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

#ifndef SMTBlinky_h
#define SMTBlinky_h
#include "Arduino.h"

// Pin Assignments

#define SMTBLINKY_COL_ENABLE_PIN PB2
#define SMTBLINKY_COL_ENABLE_BIT 0b00000100

#define SMTBLINKY_CLK_PIN PB3
#define SMTBLINKY_CLK_BIT 0b00001000

#define SMTBLINKY_DAT_PIN PB4
#define SMTBLINKY_DAT_BIT 0b00010000



class SMTBlinky
{
  public:
    void Setup(void);  // call once
    void Loop(unsigned char); // call continuously
    enum mode { 
      // Loop(mode) where mode =
      LoopModeAllChars      = 1 ,
      LoopModeNumbers       = 2,
      LoopModeScrollMessage = 3 ,
      LoopModePattern       = 4
    };
    void SetScrollingText(char *); // sets the text we are going to show

private:
    void WriteCol(unsigned char, unsigned char); 
    void *WriteNextCol(void);
    char *txt_headp, *txt_curp;

    unsigned char scrollrate_ctr;
    unsigned char scrollrate_div;
#define SMTBLINKY_SCROLLRATE_VAL 10

    unsigned char dwell_ctr;
    unsigned char dwell_div;
#define SMTBLINKY_DWELL_VAL 100

    unsigned char col_ctr;
    unsigned char coldata[5];
    unsigned char scrollstep;

}; 



#endif // SMTBlinky_h
