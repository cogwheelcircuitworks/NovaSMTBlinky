//  NovaDotMatrixCommands.h
//
//  API between NovaDotMatrix and driving host
//

/*
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
const uint8_t ndotm_cmd_escape_code=0x27;
enum ndotm_command { 
  ndotm_cmd_reset = 1,        // reset

  ndotm_cmd_message,     // set scrolling message
  ndotm_cmd_flip,        // set display upside down
  ndotm_cmd_noflip,      // set display right-side updown
  ndotm_cmd_font,        // switch font
  ndotm_cmd_dwell,       // set scroll dwell
  ndotm_cmd_rate,        // set scroll rate
  ndotm_cmd_transition,  // set scroll rate
  ndotm_cmd_data,        // load dot matrix bit by bit
  ndotm_cmd_data_scroll, // load one column and scroll the over
  ndotm_cmd_char,        // character data
  ndotm_cmd_shift_dir,   // shift data direction
  ndotm_cmd_2ch,         // write 2 small characters
  ndotm_cmd_2ch_flipped, // write 2 small characters flipped

  ndotm_cmd_max,              // marker for last command
};
