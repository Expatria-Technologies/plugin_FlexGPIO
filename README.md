# plugin_FlexGPIO

A [grblHAL](https://github.com/grblHAL) plugin that integrates the FlexiGPIO RP2040-based I2C GPIO expander, adding a large bank of digital inputs and outputs to grblHAL-based CNC controllers over a simple two-wire I2C bus.

An example implementation that currently runs on the FlexiHAL2350 board can be found here:  
https://github.com/Expatria-Technologies/FlexGPIO

---

## Overview

The FlexiGPIO expander communicates over I2C and exposes:

- **Up to 8 digital inputs:**
  - tool sensor, probe, and per-axis motor fault signals
- **Up to 16 digital outputs:**
  - auxiliary outputs (24 V and 5 V), spindle enable/direction, mist, coolant, and per-axis motor disable signals

The exact number of inputs and outputs scales automatically with `N_ABC_MOTORS` (the number of A/B/C axes configured in your grblHAL build).

All expander pins appear in grblHAL's IO port system exactly like native GPIO, making them available to spindle, coolant, probing, and auxiliary I/O subsystems without any special handling.

### Input Pin Map

| Index | Signal | Expander Pin |
|-------|--------|-------------|
| 0 | Tool | 3 |
| 1 | Probe | 4 |
| 2 | Motor_Fault_X | 5 |
| 3 | Motor_Fault_Y | 6 |
| 4 | Motor_Fault_Z | 7 |
| 5 *(if N_ABC_MOTORS ≥ 1)* | Motor_Fault_A | 8 |
| 6 *(if N_ABC_MOTORS ≥ 2)* | Motor_Fault_B | 9 |
| 7 *(if N_ABC_MOTORS = 3)* | Motor_Fault_C | 10 |

### Output Pin Map

| Index | Signal | Expander Pin | Voltage |
|-------|--------|-------------|---------|
| 0 | AUXOUT_0 | 23 | 24 V |
| 1 | AUXOUT_1 | 22 | 24 V |
| 2 | AUXOUT_2 | 21 | 24 V |
| 3 | AUXOUT_3 | 20 | 24 V |
| 4 | AUXOUT_4 | 19 | 5 V |
| 5 | AUXOUT_5 | 18 | 5 V |
| 6 | AUXOUT_6 | 17 | 5 V |
| 7 | AUXOUT_7 | 16 | 5 V |
| 8 | SPINDLE_EN | 11 | — |
| 9 | SPINDLE_DIR | 12 | — |
| 10 | MIST | 13 | — |
| 11 | COOLANT | 14 | — |
| 12 | DISABLE_X_N | 29 | — |
| 13 | DISABLE_Y_N | 28 | — |
| 14 | DISABLE_Z_N | 27 | — |
| 15 *(if N_ABC_MOTORS ≥ 1)* | DISABLE_A_N | 26 | — |
| 16 *(if N_ABC_MOTORS ≥ 2)* | DISABLE_B_N | 25 | — |
| 17 *(if N_ABC_MOTORS = 3)* | DISABLE_C_N | 24 | — |

---

## Installation

### Standard drivers (STM32, iMXRT, SAM3X8E, etc.)

1. Download or clone this repository and copy `flexgpio.c` and `flexgpio.h` into the same folder as your driver's `driver.c`.

2. Add the following to your `my_machine.h`:

```c
#define FLEXGPIO_ENABLE 1
```

3. Ensure your driver calls `flexgpio_init()` on startup, typically inside an `#if FLEXGPIO_ENABLE` guard in `driver.c`.

### CMake-based drivers (RP2040, RP2350)

Add this repository as a subdirectory and link against the `flexgpio` target:

```cmake
add_subdirectory(path/to/plugin_FlexGPIO)
target_link_libraries(your_target flexgpio)
```

Then enable the plugin in your board configuration:

```c
#define FLEXGPIO_ENABLE 1
```

---

## Configuration

### I2C address

The default I2C address is `0x48`. To override it, define the following before the plugin is compiled:

```c
#define FLEXGPIO_ADDRESS 0x3C  // example alternative address
```

### IRQ pin (optional but recommended)

To receive asynchronous input-change notifications rather than polling, define the host MCU port number connected to the expander's interrupt output:

```c
#define FLEXGPIO_IRQ_PIN  <port_number>
```

When defined, the plugin claims that port as an input and registers a change-interrupt handler. Each interrupt triggers an immediate I2C read of all input states and fires any registered grblHAL interrupt callbacks.

---

## Input Capabilities

All expander inputs support:

- Edge interrupt detection (rising, falling, or change) via the IRQ pin
- Inverted polarity
- Pull-up mode
- Claimable by grblHAL subsystems (probe, motor fault, aux)

---

## Error Handling

If the I2C expander is not found at startup, the plugin skips registration silently — no ports are added and no error is reported. If a communication error occurs during operation, `Alarm_ExpanderException` is raised, halting the machine and requiring a reset.

---

## Version

Current plugin version: **0.03**

---

## License

Released under the [GNU General Public License v3](LICENSE), consistent with the grblHAL project.

Copyright (c) 2018–2026 Terje Io  
Copyright (c) 2025 Expatria Technologies Inc.  
Copyright (c) 2026 Mitchell Grams
