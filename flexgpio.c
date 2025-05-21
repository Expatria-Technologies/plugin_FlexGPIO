/*

  flexgpio.c - driver code

  I2C I/O expander, RP2040 based FlexiGPIO

  Part of grblHAL

  Copyright (c) 2018-2023 Terje Io

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

#include "driver.h"

#if FLEXGPIO_ENABLE

#include "grbl/plugins.h"
#include "grbl/settings.h"
#include "flexgpio.h"
#include "i2c.h"

#define FLEXGPIO_ADDRESS (0x48)

uint32_t flexgpio_outpins = 0;
uint32_t flexgpio_direction_mask = 0;
uint32_t flexgpio_polarity_mask = 0;  //applies to inputs
uint32_t flexgpio_enable_mask = 0; //applies to inputs
static io_ports_data_t digital;
xbar_t flexgpio_aux_out[NUM_FLEXGPIO_AUXOUT] = {};
static enumerate_pins_ptr on_enumerate_pins;

static void digital_out_ll (xbar_t *output, float value)
{
    static uint32_t last_out = 0;

    bool on = value != 0.0f;

    if(flexgpio_aux_out[output->id].mode.inverted)
        on = !on;

    if(on)
        flexgpio_outpins |= (1 << output->pin);
    else
        flexgpio_outpins &= ~(1 << output->pin);

    if(last_out != flexgpio_outpins) {
 
        uint8_t cmd[4];

        // Split 32-bit mask into individual bytes
        cmd[0] = flexgpio_outpins & 0xFF;         // Least significant byte
        cmd[1] = (flexgpio_outpins >> 8) & 0xFF;  // Second byte
        cmd[2] = (flexgpio_outpins >> 16) & 0xFF; // Third byte
        cmd[3] = (flexgpio_outpins >> 24) & 0xFF; // Most significant byte
        last_out = flexgpio_outpins;

        while (!i2c_send(FLEXGPIO_ADDRESS, cmd, 4, false)){
            hal.delay_ms(1, NULL);
        }
    }
}

static bool digital_out_cfg (xbar_t *output, gpio_out_config_t *config, bool persistent)
{
    if(output->id == 1) {

        if(config->inverted != flexgpio_aux_out[output->id].mode.inverted) {
            flexgpio_aux_out[output->id].mode.inverted = config->inverted;
            digital_out_ll(output, (float)(!(flexgpio_outpins & (1 << output->pin)) ^ config->inverted));
        }

        // Open drain not supported

        if(persistent)
            ioport_save_output_settings(output, config);
    }

    return output->id < digital.out.n_ports;
}

static void digital_out (uint8_t port, bool on)
{
    if(port < digital.out.n_ports)
        digital_out_ll(&flexgpio_aux_out[port], (float)on);
}

static float digital_out_state (xbar_t *output)
{
    float value = -1.0f;

    if(output->id < digital.out.n_ports)
        value = (float)((flexgpio_outpins & (1 << output->pin)) != 0);

    return value;
}

static bool set_pin_function (xbar_t *output, pin_function_t function)
{
    if(output->id < digital.out.n_ports)
        flexgpio_aux_out[output->id].function = function;

    return output->id < digital.out.n_ports;
}

static xbar_t *get_pin_info (io_port_direction_t dir, uint8_t port)
{
    static xbar_t pin;

    xbar_t *info = NULL;

    if(dir == Port_Output && port < digital.out.n_ports) {
        memcpy(&pin, &flexgpio_aux_out[port], sizeof(xbar_t));
        pin.get_value = digital_out_state;
        pin.set_value = digital_out_ll;
        pin.set_function = set_pin_function;
        pin.config = digital_out_cfg;
        info = &pin;
    }

    return info;
}

static void set_pin_description (io_port_direction_t dir, uint8_t port, const char *description)
{
    if(dir == Port_Output && port < digital.out.n_ports)
        flexgpio_aux_out[port].description = description;
}

static void onEnumeratePins (bool low_level, pin_info_ptr pin_info, void *data)
{
    static xbar_t pin = {};

    on_enumerate_pins(low_level, pin_info, data);

    uint_fast8_t idx;

    for(idx = 0; idx < digital.out.n_ports; idx ++) {

        memcpy(&pin, &flexgpio_aux_out[idx], sizeof(xbar_t));

        if(!low_level)
            pin.port = "FLEXGPIO:";

        pin_info(&pin, data);
    };
}

static void get_aux_max (xbar_t *pin, void *fn)
{
    if(pin->group == PinGroup_AuxOutput)
        *(pin_function_t *)fn = max(*(pin_function_t *)fn, pin->function + 1);
}

/*
flexgpio_t pins flexgpio_in (void)
{
    flexgpio_t pins = {0};
    uint8_t cmd[4] = {0}; // Use 4 bytes to match 32-bit uint32_t
    //cmd[0] = READ_INPUT;
    
    i2c_receive(FLEXGPIO_ADDRESS, cmd, 4, true);
    
    // Convert received bytes to 32-bit value
    pins.data_mask = (cmd[3] << 24) | (cmd[2] << 16) | (cmd[1] << 8) | cmd[0];
    
    return pins;

}*/

void flexgpio_init (void)
{
    if(i2c_start().ok && i2c_probe(FLEXGPIO_ADDRESS)) {

        //settings.probe.invert_probe_pin;
        //settings.probe.invert_toolsetter_input;
        
        uint_fast8_t idx;
        uint8_t cmd[16];
        pin_function_t aux_out_base = Output_Aux0;

        io_digital_t dports = {
            .ports = &digital,
            .digital_out = digital_out,
            .get_pin_info = get_pin_info,
            .set_pin_description = set_pin_description,
        };

        hal.enumerate_pins(false, get_aux_max, &aux_out_base);
        
        digital.out.n_ports = sizeof(flexgpio_aux_out) / sizeof(xbar_t);
        
        //pins are set up in board init as they are board specific

        ioports_add_digital(&dports);

        // Split 32-bit mask into individual bytes
        cmd[0] = flexgpio_outpins & 0xFF;         // Least significant byte
        cmd[1] = (flexgpio_outpins >> 8) & 0xFF;  // Second byte
        cmd[2] = (flexgpio_outpins >> 16) & 0xFF; // Third byte
        cmd[3] = (flexgpio_outpins >> 24) & 0xFF; // Most significant byte

        cmd[4] = flexgpio_direction_mask & 0xFF;         // Least significant byte
        cmd[5] = (flexgpio_direction_mask >> 8) & 0xFF;  // Second byte
        cmd[6] = (flexgpio_direction_mask >> 16) & 0xFF; // Third byte
        cmd[7] = (flexgpio_direction_mask >> 24) & 0xFF; // Most significant byte
        
        cmd[8] = flexgpio_polarity_mask & 0xFF;         // Least significant byte
        cmd[9] = (flexgpio_polarity_mask >> 8) & 0xFF;  // Second byte
        cmd[10] = (flexgpio_polarity_mask >> 16) & 0xFF; // Third byte
        cmd[11] = (flexgpio_polarity_mask >> 24) & 0xFF; // Most significant byte
        
        cmd[12] = flexgpio_enable_mask & 0xFF;         // Least significant byte
        cmd[13] = (flexgpio_enable_mask >> 8) & 0xFF;  // Second byte
        cmd[14] = (flexgpio_enable_mask >> 16) & 0xFF; // Third byte
        cmd[15] = (flexgpio_enable_mask >> 24) & 0xFF; // Most significant byte

        while (!i2c_send(FLEXGPIO_ADDRESS, cmd, 16, false)){
            hal.delay_ms(1, NULL);
        }

        on_enumerate_pins = hal.enumerate_pins;
        hal.enumerate_pins = onEnumeratePins;
    }
}

#endif
