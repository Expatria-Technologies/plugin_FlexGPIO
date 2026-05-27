# plugin_FlexGPIO

A [grblHAL](https://github.com/grblHAL) plugin that integrates the FlexiGPIO RP2040-based I2C GPIO expander, adding a large bank of digital inputs and outputs to grblHAL-based CNC controllers over a simple two-wire I2C bus.

An example implementation that currently runs on the FlexiHAL2350 board can be found here:  
https://github.com/Expatria-Technologies/FlexGPIO

---

## Overview

The FlexiGPIO expander communicates over I2C and exposes:

- **Up to 8 digital inputs** — tool sensor, probe, and per-axis motor fault signals
- **Up to 16 digital outputs** — auxiliary outputs (24 V and 5 V), spindle enable/direction, mist, coolant, and per-axis motor disable signals

The exact number of inputs and outputs scales automatically with `N_ABC_MOTORS` (the number of A/B/C axes configured in your grblHAL build).

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

## I2C Protocol

The plugin communicates with the expander using raw I2C byte sequences.

**Output write (4 bytes):** The current 32-bit output state is sent as four bytes, least-significant byte first.

**Configuration write (8 bytes):** Sent at startup and whenever IRQ masks change:
- Bytes 0–3: current output state (as above)
- Bytes 4–5: MCU IRQ mask (16-bit, LSB first) — pins whose edge events are routed to the host MCU
- Bytes 6–7: Probe IRQ mask (16-bit, LSB first) — pins configured for probe interrupt-all mode

**Input read (4 bytes):** Four bytes are read back from the expander and assembled into a 32-bit pin state word (LSB first).

If any I2C transaction fails, the plugin raises `Alarm_ExpanderException` to halt motion safely.

---

## Configuration

### Enable the plugin

In your board map or `CMakeLists.txt`, define:

```c
#define FLEXGPIO_ENABLE 1
```

### I2C address

The default I2C address is `0x48`. To override it, define before including the plugin:

```c
#define FLEXGPIO_ADDRESS 0x3C  // example alternative address
```

### IRQ pin (optional but recommended)

To receive asynchronous input change notifications rather than polling, define the MCU GPIO port number connected to the expander's interrupt output:

```c
#define FLEXGPIO_IRQ_PIN  <port_number>
```

When defined, the plugin claims that port as an input and registers a change-interrupt handler. Each interrupt triggers an immediate I2C read of all input states and fires any registered grblHAL interrupt callbacks.

---

## Building

The plugin is provided as a CMake interface library. Add it to your grblHAL build by including the directory and linking against the `flexgpio` target:

```cmake
add_subdirectory(path/to/plugin_FlexGPIO)
target_link_libraries(your_target flexgpio)
```

The `CMakeLists.txt` in this repository handles the source and include paths automatically.

---

## Integration

Call `flexgpio_init()` from your driver's initialisation code (typically `driver.c` or equivalent, gated on `FLEXGPIO_ENABLE`):

```c
#if FLEXGPIO_ENABLE
#include "flexgpio.h"
...
flexgpio_init();
#endif
```

`flexgpio_init()` will:
1. Optionally claim the MCU IRQ pin.
2. Probe the I2C bus for the expander at the configured address.
3. Register all digital inputs and outputs with grblHAL's `ioports` subsystem, making them available to the rest of the system (spindle, coolant, probing, aux I/O, etc.) exactly like native GPIO pins.
4. Send an initial configuration packet and read back the starting input states.

Pins appear in grblHAL pin enumeration with the port name `FLEXGPIO:`.

---

## Input Capabilities

All expander inputs support:

- Edge interrupt detection (rising, falling, or change) via the MCU IRQ pin
- Inverted polarity
- Pull-up mode
- Claimable by grblHAL subsystems (probe, motor fault, aux)

---

## Error Handling

If the I2C expander is unreachable at startup, the plugin silently skips registration (no ports are added). If a communication error occurs during operation, `Alarm_ExpanderException` is raised, which halts the machine and requires a reset — the same behaviour as other grblHAL I/O errors.

---

## Version

Current plugin version: **0.03**

---

## License

This plugin is released under the [GNU General Public License v3](LICENSE), consistent with the grblHAL project.

Copyright (c) 2018–2026 Terje Io  
Copyright (c) 2025 Expatria Technologies Inc.  
Copyright (c) 2026 Mitchell Grams
