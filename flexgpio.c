/*

  flexgpio.c - driver code for FLEXGPIO I2C expander

  Part of grblHAL

  Copyright (c) 2018-2026 Terje Io
  Copyright (c) 2025 Expatria Technologies Inc.
  Copyright (c) 2026 Mitchell Grams

  grblHAL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  grblHAL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with grblHAL. If not, see <http://www.gnu.org/licenses/>.

*/

#include "driver.h"

#if FLEXGPIO_ENABLE == 1

#include <stdio.h>
#include <math.h>

#include "grbl/task.h"
#include "grbl/protocol.h"
#include "grbl/utf8.h"
#include "flexgpio.h"

#ifndef FLEXGPIO_ADDRESS
#define FLEXGPIO_ADDRESS (0x48)
#endif

//TODO: consider if the below is redundant with board map and could be simplified

static const uint8_t flexgpio_in_map[] = {
    3,      // Tool
    4,      // Probe
    5,      // Motor_Fault_X
    6,      // Motor_Fault_Y
    7,      // Motor_Fault_Z
#if N_ABC_MOTORS > 0
    8,      // Motor_Fault_A
#endif
#if N_ABC_MOTORS >= 2
    9,      // Motor_Fault_B
#endif
#if N_ABC_MOTORS == 3
    10      // Motor_Fault_C
#endif
};

static const uint8_t flexgpio_out_map[] = {
    23, // AUXOUT_0 (24V)
    22, // AUXOUT_1 (24V)
    21, // AUXOUT_2 (24V)
    20, // AUXOUT_3 (24V)
    19, // AUXOUT_4 (5V)
    18, // AUXOUT_5 (5V)
    17, // AUXOUT_6 (5V)
    16, // AUXOUT_7 (5V)

    11, // SPINDLE_EN
    12, // SPINDLE_DIR
    13, // MIST
    14, // COOLANT

    29, // DISABLE_X_N
    28, // DISABLE_Y_N
    27, // DISABLE_Z_N
#if N_ABC_MOTORS > 0
    26, // DISABLE_A_N
#endif
#if N_ABC_MOTORS >= 2
    25, // DISABLE_B_N
#endif
#if N_ABC_MOTORS == 3
    24  // DISABLE_C_N
#endif
};

#define FLEXGPIO_N_DIN   (sizeof(flexgpio_in_map) / sizeof(flexgpio_in_map[0]))
#define FLEXGPIO_N_DOUT  (sizeof(flexgpio_out_map) / sizeof(flexgpio_out_map[0]))

static struct {
    pin_irq_mode_t mode;
    ioport_interrupt_callback_ptr callback;
} irq[FLEXGPIO_N_DIN] = {};

static xbar_t aux_in[FLEXGPIO_N_DIN] = {};
static xbar_t aux_out[FLEXGPIO_N_DOUT] = {};
static io_ports_data_t digital;
static uint32_t d_out = 0, d_in = 0;
static volatile uint32_t event_bits = 0;

static driver_reset_ptr driver_reset;
static enumerate_pins_ptr on_enumerate_pins;
static on_report_options_ptr on_report_options;

static uint32_t last_out = 0;
static uint16_t probe_irq_mask = 0;
static uint16_t mcu_irq_mask = 0;

static void flexgpio_config (void *data);

static void digital_out_ll (xbar_t *output, float value)
{

    bool on = value != 0.0f;

    if(aux_out[output->id].mode.inverted)
        on = !on;

    if(on)
        *(uint32_t *)output->port |= (1 << output->pin);
    else
        *(uint32_t *)output->port &= ~(1 << output->pin);

    if(last_out != *(uint32_t *)output->port) {
        last_out = *(uint32_t *)output->port;

        uint8_t cmd[4];

        // Split 32-bit mask into individual bytes
        cmd[0] = last_out & 0xFF;         // Least significant byte
        cmd[1] = (last_out >> 8) & 0xFF;  // Second byte
        cmd[2] = (last_out >> 16) & 0xFF; // Third byte
        cmd[3] = (last_out >> 24) & 0xFF; // Most significant byte

        if(!i2c_send(FLEXGPIO_ADDRESS, cmd, 4, true)){
            system_raise_alarm(Alarm_ExpanderException);
        }
    }
}

static bool digital_out_cfg (xbar_t *output, gpio_out_config_t *config, bool persistent)
{
    if(output->id < digital.out.n_ports) {

        if(config->inverted != aux_out[output->id].mode.inverted) {
            aux_out[output->id].mode.inverted = config->inverted;
            digital_out_ll(output, (float)(!(*(uint32_t *)output->port & (1 << output->pin)) ^ config->inverted));        }
        // Open drain not supported

        if(persistent)
            ioport_save_output_settings(output, config);
    }

    return output->id < digital.out.n_ports;
}

static void digital_out (uint8_t port, bool on)
{
    if(port < digital.out.n_ports)
        digital_out_ll(&aux_out[port], (float)on);
}

static float digital_out_state (xbar_t *output)
{
    float value = -1.0f;

    if(output->id < digital.out.n_ports)
        value = (float)(!!(*(uint32_t *)output->port & (1 << output->pin)));

    return value;
}

static bool digital_in_cfg (xbar_t *input, gpio_in_config_t *config, bool persistent)
{
    if(input->id < digital.in.n_ports && config->pull_mode != PullMode_UpDown) {

        if(!xbar_is_probe_in(input->function) && !xbar_is_motor_fault_in(input->function))
            aux_in[input->id].mode.inverted = config->inverted;

        if(xbar_is_probe_in(input->function)){
            mcu_irq_mask &= ~(1 << input->pin); // Clear MCU IRQ bit
            task_add_immediate(flexgpio_config, NULL);
        }
        
        if(persistent)
            ioport_save_input_settings(input, config);
    }

    return input->id < digital.in.n_ports;
}

static float digital_in_state (xbar_t *input)
{
    float value = -1.0f;

    if(input->id < digital.in.n_ports)
        value = (float)(((*(uint32_t *)input->port & (1 << input->pin)) != 0) ^ aux_in[input->id].mode.inverted);

    return value;
}

inline static __attribute__((always_inline)) int32_t get_input (const xbar_t *input, wait_mode_t wait_mode, float timeout)
{
    if(wait_mode == WaitMode_Immediate)
        return !!(*(uint32_t *)input->port & (1 << input->pin)) ^ input->mode.inverted;

    int32_t value = -1;
    uint32_t mask = 1 << input->pin;
    uint_fast16_t delay = (uint_fast16_t)ceilf((1000.0f / 50.0f) * timeout) + 1;

    if(wait_mode == WaitMode_Rise || wait_mode == WaitMode_Fall) {

        pin_irq_mode_t mode = wait_mode == WaitMode_Rise ? IRQ_Mode_Rising : IRQ_Mode_Falling;

        if(input->cap.irq_mode & mode) {

            event_bits &= ~mask;
            irq[input->id].mode = mode;

            do {
                if(event_bits & mask) {
                    value = !!(*(uint32_t *)input->port & mask) ^ input->mode.inverted;
                    break;
                }
                if(delay) {
                    protocol_execute_realtime();
                    hal.delay_ms(50, NULL);
                } else
                    break;
            } while(--delay && !sys.abort);

            irq[input->id].mode = input->mode.irq_mode;    // Restore pin interrupt status
        }

    } else {

        bool wait_for = wait_mode != WaitMode_Low;

        do {
            if((!!(*(uint32_t *)input->port & mask) ^ input->mode.inverted) == wait_for) {
                value = wait_for;
                break;
            }
            if(delay) {
                protocol_execute_realtime();
                hal.delay_ms(50, NULL);
            } else
                break;
        } while(--delay && !sys.abort);
    }

    return value;
}

static int32_t wait_on_input (uint8_t port, wait_mode_t wait_mode, float timeout)
{
    int32_t value = -1;

    if(port < digital.in.n_ports)
        value = get_input(&aux_in[port], wait_mode, timeout);

    return value;
}

static bool register_interrupt_handler (uint8_t port, uint8_t user_port, pin_irq_mode_t irq_mode, ioport_interrupt_callback_ptr interrupt_callback)
{
    bool ok;

    if((ok = port < digital.in.n_ports && aux_in[port].cap.irq_mode != IRQ_Mode_None)) {

        xbar_t *input = &aux_in[port];

        if(ok = (irq_mode == IRQ_Mode_All) && xbar_is_probe_in(input->function)){
            probe_irq_mask |= 1 << input->pin; // Set probe IRQ bit
        }
        else if((ok = (irq_mode & input->cap.irq_mode) == irq_mode && interrupt_callback != NULL)) {
            irq[input->id].callback = interrupt_callback;
            irq[input->id].mode = input->mode.irq_mode = irq_mode;
        }

        if(irq_mode == IRQ_Mode_None || !ok) {
            irq[input->id].callback = NULL;
            irq[input->id].mode = input->mode.irq_mode = IRQ_Mode_None;
            probe_irq_mask &= ~(1 << input->pin); // Clear probe IRQ bit
        }
    }

    task_add_immediate(flexgpio_config, NULL); //OKAY TO CALL THIS ALWAYS?

    return ok;
}

static bool set_pin_function (xbar_t *port, pin_function_t function)
{
    if(port->mode.input)
        aux_in[port->id].function = function;
    else
        aux_out[port->id].function = function;

    return true;
}

static void set_pin_description (io_port_direction_t dir, uint8_t port, const char *description)
{
    if(dir == Port_Input && port < digital.in.n_ports)
        aux_in[port].description = description;
    else if(dir == Port_Output && port < digital.out.n_ports)
        aux_out[port].description = description;
}

static xbar_t *get_pin_info (io_port_direction_t dir, uint8_t port)
{
    static xbar_t pin;

    xbar_t *info = NULL;

    if(dir == Port_Input && port < digital.in.n_ports) {
        memcpy(&pin, &aux_in[port], sizeof(xbar_t));
        //pin.pin += digital.in.n_start;
        pin.get_value = digital_in_state;
        pin.set_function = set_pin_function;
        pin.config = digital_in_cfg;
        info = &pin;
    } else if(dir == Port_Output && port < digital.out.n_ports) {
        memcpy(&pin, &aux_out[port], sizeof(xbar_t));
        //pin.pin += digital.out.n_start;
        pin.get_value = digital_out_state;
        pin.set_value = digital_out_ll;
        pin.set_function = set_pin_function;
        pin.config = digital_out_cfg;
        info = &pin;
    }

    return info;
}

static void i2c_get_inputs (void *data)
{
    uint32_t pins;

    uint8_t cmd[4] = {0}; // Use 4 bytes to match 32-bit uint32_t
   
    if (!i2c_receive(FLEXGPIO_ADDRESS, cmd, 4, true)){
        system_raise_alarm(Alarm_ExpanderException);
        return;
    }
    // Convert received bytes to 32-bit value
    pins = ((uint32_t)cmd[3] << 24) | ((uint32_t)cmd[2] << 16) | ((uint32_t)cmd[1] << 8) | (uint32_t)cmd[0];

    for (uint_fast8_t idx = 0; idx < digital.in.n_ports; idx ++) {

        xbar_t *input = &aux_in[idx];
    
        if(input->port) { //ALWAYS TRUE? SHOULD THIS CHECK IF PORT IS VALID?

            uint32_t event = 0, bit = 1UL << flexgpio_in_map[idx];

            switch(irq[input->id].mode) {

                case IRQ_Mode_Rising:
                    if(((pins & bit)!= 0) && !((*(uint32_t *)input->port & bit)!= 0))
                        event |= bit;
                    break;

                case IRQ_Mode_Falling:
                    if(!((pins & bit)!= 0) && ((*(uint32_t *)input->port & bit)!= 0))
                        event |= bit;
                    break;

                case IRQ_Mode_Change:
                    if(((pins & bit)!= 0) != ((*(uint32_t *)input->port & bit)!= 0))
                        event |= bit;
                    break;

                default: break;
            }

            if((pins & bit) != 0)
                *(uint32_t *)input->port |= bit;
            else
                *(uint32_t *)input->port &= ~bit;

            if((event & bit) && irq[input->id].callback)
                    irq[input->id].callback(digital.in.n_start + input->id, !!(*(uint32_t *)input->port & bit));

            event_bits |= event; // Should this be reset somewhere? Is it okay for pending events to persist indefinitely.
        }
    }
}

ISR_CODE static void ISR_FUNC(flexgpio_response)(uint8_t port, bool state)
{
    task_add_immediate(i2c_get_inputs, NULL);
}

static void get_aux_out_max (xbar_t *pin, void *fn)
{
    if(pin->group == PinGroup_AuxOutput)
        *(pin_function_t *)fn = max(*(pin_function_t *)fn, pin->function + 1);
}

static void get_aux_in_max (xbar_t *pin, void *fn)
{
    if(pin->group == PinGroup_AuxInput)
        *(pin_function_t *)fn = max(*(pin_function_t *)fn, pin->function + 1);
}

static void flexgpio_config (void *data)
{
    uint8_t cmd[8];
    //OUTPUT VALUES
    cmd[0] = last_out & 0xFF;               // Least significant byte
    cmd[1] = (last_out >> 8) & 0xFF;        // Second byte
    cmd[2] = (last_out >> 16) & 0xFF;       // Third byte
    cmd[3] = (last_out >> 24) & 0xFF;       // Most significant byte
    //MCU_IRQ_MASK
    cmd[4] = mcu_irq_mask & 0xFF;           // Least significant byte
    cmd[5] = (mcu_irq_mask >> 8) & 0xFF;    // Most significant byte
    //PROBE_IRQ_MASK
    cmd[6] = probe_irq_mask & 0xFF;         // Least significant byte
    cmd[7] = (probe_irq_mask >> 8) & 0xFF;  // Most significant byte

    // send configuration info
    if(!i2c_send(FLEXGPIO_ADDRESS, cmd, 8, true)){
        system_raise_alarm(Alarm_ExpanderException);
    }

    // update input states
    task_add_immediate(i2c_get_inputs, NULL);
}

static void driverReset (void)
{
    // todo: deterministic pin states (what happens if there was disconnect?)
    driver_reset();
}

static void onEnumeratePins (bool low_level, pin_info_ptr pin_info, void *data)
{
    static xbar_t pin = {};

    on_enumerate_pins(low_level, pin_info, data);

    uint_fast8_t idx;

    for(idx = 0; idx < digital.in.n_ports; idx ++) {

        memcpy(&pin, &aux_in[idx], sizeof(xbar_t));

        if(!low_level)
            pin.port = "FLEXGPIO:";

        pin_info(&pin, data);
    }

    for(idx = 0; idx < digital.out.n_ports; idx ++) {

        memcpy(&pin, &aux_out[idx], sizeof(xbar_t));

        if(!low_level)
            pin.port = "FLEXGPIO:";

        pin_info(&pin, data);
    }
}

static void onReportOptions (bool newopt)
{
    on_report_options(newopt);

    if(!newopt)
        report_plugin("FLEXGPIO", "0.03");
}

void flexgpio_init (void)
{
    uint_fast8_t idx = 0;
    pin_function_t aux_in_base = Input_Aux0, aux_out_base = Output_Aux0;

    io_digital_t dports = {
        .ports = &digital,
        .digital_out = digital_out,
        .get_pin_info = get_pin_info,
        .wait_on_input = wait_on_input,
        .set_pin_description = set_pin_description,
        .register_interrupt_handler = register_interrupt_handler
    };

    on_report_options = grbl.on_report_options;
    grbl.on_report_options = onReportOptions;

    #if FLEXGPIO_IRQ_PIN

        uint8_t expander_irq_port = FLEXGPIO_IRQ_PIN;
        io_port_cfg_t mcu_d_in;
        xbar_t *portinfo;

        if(ioports_cfg(&mcu_d_in, Port_Digital, Port_Input) && (portinfo = mcu_d_in.claim(&mcu_d_in, &expander_irq_port, "FlexGPIO MCU IRQ", (pin_cap_t){ .irq_mode = IRQ_Mode_Change})))
            ioport_enable_irq(expander_irq_port, IRQ_Mode_Change, flexgpio_response);
        else
            task_run_on_startup(report_warning, "FlexGPIO plugin failed to claim port for MCU IRQ!");

    #endif // FLEXGPIO_IRQ_PIN

    if(i2c_start().ok && i2c_probe(FLEXGPIO_ADDRESS)) {

        driver_reset = hal.driver_reset;
        hal.driver_reset = driverReset;

        hal.enumerate_pins(false, get_aux_in_max, &aux_in_base);
        hal.enumerate_pins(false, get_aux_out_max, &aux_out_base);

        digital.in.n_ports = FLEXGPIO_N_DIN;

        for(idx = 0; idx < digital.in.n_ports; idx++) {
            aux_in[idx].id = idx;
            aux_in[idx].pin = flexgpio_in_map[idx];
            aux_in[idx].port = &d_in;
            aux_in[idx].function = aux_in_base + idx;
            aux_in[idx].group = PinGroup_AuxInput;
            aux_in[idx].cap.input = On;
            aux_in[idx].cap.irq_mode = IRQ_Mode_Edges;
            aux_in[idx].cap.pull_mode = PullMode_Up;
            aux_in[idx].cap.external = On;
            aux_in[idx].cap.claimable = On;
            aux_in[idx].cap.invert = On;
            aux_in[idx].mode.input = On;

            mcu_irq_mask |= 1 << aux_in[idx].pin;
        }

        digital.out.n_ports = FLEXGPIO_N_DOUT;

        for(idx = 0; idx < digital.out.n_ports; idx++) {
            aux_out[idx].id = idx;
            aux_out[idx].pin = flexgpio_out_map[idx];
            aux_out[idx].port = &d_out;
            aux_out[idx].function = aux_out_base + idx;
            aux_out[idx].group = PinGroup_AuxOutput;
            aux_out[idx].cap.output = On;
            aux_out[idx].cap.external = On;
            aux_out[idx].cap.claimable = On;
            aux_out[idx].cap.invert = On;
            aux_out[idx].mode.output = On;
        }

        ioports_add_digital(&dports);

        on_enumerate_pins = hal.enumerate_pins;
        hal.enumerate_pins = onEnumeratePins;

        task_run_on_startup(flexgpio_config, NULL);
    }
}

#endif // FLEXGPIO_ENABLE