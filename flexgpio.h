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

typedef struct {
    uint32_t data_mask;   // Bitmask for actual pin states
    uint32_t rw_mask;     // Bitmask for Read/Write configuration (0 = input, 1 = output)
    uint32_t inv_mask;    // Bitmask for inversion configuration
    
    struct {
        uint32_t data0 :1,
                 data1 :1,
                 data2 :1,
                 data3 :1,
                 data4 :1,
                 data5 :1,
                 data6 :1,
                 data7 :1,
                 data8 :1,
                 data9 :1,
                 data10 :1,
                 data11 :1,
                 data12 :1,
                 data13 :1,
                 data14 :1,
                 data15 :1,
                 data16 :1,
                 data17 :1,
                 data18 :1,
                 data19 :1,
                 data20 :1,
                 data21 :1,
                 data22 :1,
                 data23 :1,
                 data24 :1,
                 data25 :1,
                 data26 :1,
                 data27 :1,
                 data28 :1,
                 data29 :1,
                 data30 :1,
                 data31 :1;
    } data;

    struct {
        uint32_t rw0 :1,
                 rw1 :1,
                 rw2 :1,
                 rw3 :1,
                 rw4 :1,
                 rw5 :1,
                 rw6 :1,
                 rw7 :1,
                 rw8 :1,
                 rw9 :1,
                 rw10 :1,
                 rw11 :1,
                 rw12 :1,
                 rw13 :1,
                 rw14 :1,
                 rw15 :1,
                 rw16 :1,
                 rw17 :1,
                 rw18 :1,
                 rw19 :1,
                 rw20 :1,
                 rw21 :1,
                 rw22 :1,
                 rw23 :1,
                 rw24 :1,
                 rw25 :1,
                 rw26 :1,
                 rw27 :1,
                 rw28 :1,
                 rw29 :1,
                 rw30 :1,
                 rw31 :1;
    } rw;

    struct {
        uint32_t inv0 :1,
                 inv1 :1,
                 inv2 :1,
                 inv3 :1,
                 inv4 :1,
                 inv5 :1,
                 inv6 :1,
                 inv7 :1,
                 inv8 :1,
                 inv9 :1,
                 inv10 :1,
                 inv11 :1,
                 inv12 :1,
                 inv13 :1,
                 inv14 :1,
                 inv15 :1,
                 inv16 :1,
                 inv17 :1,
                 inv18 :1,
                 inv19 :1,
                 inv20 :1,
                 inv21 :1,
                 inv22 :1,
                 inv23 :1,
                 inv24 :1,
                 inv25 :1,
                 inv26 :1,
                 inv27 :1,
                 inv28 :1,
                 inv29 :1,
                 inv30 :1,
                 inv31 :1;
    } inv;

} flexgpio_t;

#endif
#endif