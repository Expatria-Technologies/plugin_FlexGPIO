/*

  flexgpio.h - driver code for RP2040 ARM processor

  I2C I/O expander

  Part of grblHAL

  Copyright (c) 2018-2022 Terje Io

  Grbl is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Grbl is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Grbl.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef _FLEXGPIO_H_
#define _FLEXGPIO_H_

#include "driver.h"

/*
#define ioex_out(pin) ioex_outN(pin)
#define ioex_outN(pin) io_expander.out ## pin
#define ioex_in(pin) ioex_inN(pin)
#define ioex_inN(pin) io_expander.in ## pin
*/

#if FLEXGPIO_ENABLE

/*typedef struct {
    uint32_t out_mask;   // Bitmask for Write states
    uint32_t in_mask;    // Bitmask for Read states
    uint32_t rw_mask;    // Bitmask for Read/Write configuration (0 = input, 1 = output)
    uint32_t inv_mask;   // Bitmask for inversion configuration

    // Individual bit access using union and struct
    union {
        uint32_t value;
        struct {
            uint32_t bit[32]; // 32 bits, mapped to each GPIO pin
        };
    } out, in, rw, inv;
} flexgpio_t;*/

extern xbar_t flexgpio_aux_out[NUM_FLEXGPIO_AUXOUT];
extern uint32_t flexgpio_outpins;
extern uint32_t flexgpio_direction_mask;
extern uint32_t flexgpio_polarity_mask;  //applies to inputs
extern uint32_t flexgpio_enable_mask; //applies to inputs

#endif
#endif
