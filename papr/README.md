# PAPR Firmware

Reference firmware for a Powered Air-Purifying Respirator (PAPR) running on
32-bit microcontrollers. Written in portable C11.

- **Revision 1** — generic 32-bit MCU, generic PWM blower stage.
- **Revision 2** — blower stage driven by an
  [ST L6235](https://www.st.com/en/motor-drivers/l6235.html) three-phase DMOS
  brushless DC driver. The MCU sets a peak-current reference; the L6235
  handles commutation and chopping.
- **Revision 3** — concrete reference port to the
  [GigaDevice GD32E517RE](https://www.gigadevice.com/microcontroller/gd32e517re/)
  (Cortex-M33 + FPU at 180 MHz, 512 KB Flash, 128 KB SRAM, LQFP64 package
  exposing 51 GPIO pins).
- **Revision 4** — differential pressure path moved to a
  [Sensirion SDP810-500Pa](https://sensirion.com/products/catalog/SDP810-500Pa/)
  (4-pin tube-connection variant, I²C address 0x25). Added a portable
  SDP810 driver with full Sensirion CRC-8 framing; the HAL now exposes raw
  I²C primitives instead of a high-level pressure call. `pressure_pa` is
  now `int16_t` since the chip reports signed differential pressure.
- **Revision 5** — adds two GigaDevice peripherals:
  - **GDY1124** absolute-pressure sensor on the same I²C bus (address 0x76).
    Provides ambient barometric reference for altitude-compensated airflow
    estimation; reading exposed via `papr_telemetry_t::absolute_pressure_pa`.
  - **GD32VW553-UNIFI-EMH7** Bluetooth LE module on USART0. A custom framed
    binary protocol (preamble `AA 55`, length, command, payload, CRC-8)
    lets a paired mobile app read live telemetry, change the flow level,
    power the unit on/off, mute alarms, and clear faults.

  All revision-1 modules and headers remain in place; revisions 2-5 only
  added or extended functionality.

## Layout

```
papr/
├── include/                Public headers (config, types, HAL contract, modules)
├── src/                    Portable controller core (incl. L6235 driver)
├── hal/
│   ├── papr_hal_stub.c     Host stub for verification builds
│   └── gd32e517re/         GigaDevice GD32E517RE port (rev 3)
│       ├── papr_pinmap.h
│       ├── papr_hal_gd32e517re.c
│       └── gd32e517re.ld
└── CMakeLists.txt
```

## Building

### Host verification (default)

```sh
cmake -S papr -B papr/build
cmake --build papr/build
```

Produces `papr/build/papr_firmware` linked against the simulation HAL stub.

### GD32E517RE target

Requires `arm-none-eabi-gcc` and the GigaDevice GD32E51x firmware library.

```sh
cmake -S papr -B papr/build-gd32 \
      -DCMAKE_TOOLCHAIN_FILE=path/to/arm-none-eabi.cmake \
      -DPAPR_TARGET=GD32E517RE \
      -DGD32E51X_SDK_DIR=path/to/GD32E51x_Firmware_Library_V1.x.x
cmake --build papr/build-gd32
```

Produces an ELF that can be flashed with GD-Link, J-Link, or OpenOCD.

## GD32E517RE pin map (LQFP64, 51 GPIO available)

| Function              | Port / Pin     | Peripheral          |
| --------------------- | -------------- | ------------------- |
| L6235 VREF            | PA4            | DAC0_OUT0           |
| L6235 EN              | PB12           | GPIO out            |
| L6235 FWD             | PB13           | GPIO out            |
| L6235 BRAKE (active L)| PB14           | GPIO out            |
| L6235 DIAG            | PB15           | EXTI15 (falling)    |
| L6235 TACHO           | PA8            | TIMER1_CH0 ext-clk  |
| Battery V (divider)   | PA0            | ADC0_IN0            |
| Battery I (shunt amp) | PA1            | ADC0_IN1            |
| Flow analog           | PA2            | ADC0_IN2            |
| NTC temperature       | PA3            | ADC0_IN3            |
| Pressure SCL/SDA      | PB6 / PB7      | I2C0                |
| Power button          | PC13           | GPIO in (pull-up)   |
| Level button          | PB0            | GPIO in (pull-up)   |
| LED OK / Warn / Fault | PB1 / PB2 / PB10 | GPIO out          |
| Buzzer                | PA6            | TIMER2_CH0 PWM      |
| UART (telemetry)      | PA9 / PA10     | USART0              |
| SWDIO / SWCLK         | PA13 / PA14    | debug               |
| HXTAL (25 MHz)        | PD0 / PD1      | RCU                 |
| LXTAL (32.768 kHz)    | PC14 / PC15    | RTC                 |

Reserved package pins (VDD, VSS, VDDA, VSSA, VBAT, NRST, BOOT0) account
for the remaining 13 of the 64 LQFP positions.

## Modules

- `papr_controller` — supervisory state machine.
- `papr_blower` — closed-loop airflow PID; output is L6235 phase current in mA.
- `papr_l6235` — L6235 chip driver. Translates current (mA) → VREF DAC code
  via `I_peak = VREF / R_sense`; drives EN/FWD/BRAKE; monitors DIAG and TACHO.
- `papr_sdp810` — portable Sensirion SDP810 driver. Issues "start continuous
  measurement, differential pressure, with averaging" (`0x36 0x1E`), reads
  9-byte frames, validates Sensirion CRC-8 over each (pressure, temperature,
  scale-factor) word, and applies the chip-reported scale factor (60 LSB/Pa
  for the 500 Pa range).
- `papr_gdy1124` — portable driver for the GigaDevice GDY1124 absolute
  pressure sensor. Reads chip-ID, the 24-byte calibration block, and 6-byte
  raw pressure/temperature samples; applies the standard 32/64-bit fixed-
  point compensation. Output: pressure in Pa, temperature in 0.1 °C.
- `papr_ble` — portable driver for the GD32VW553-UNIFI-EMH7 BLE module.
  Owns a TX serializer and a state-machine RX parser; dispatches commands
  to `papr_controller_remote_*` hooks; sends periodic telemetry frames and
  edge-triggered events for state, level, and alarm-mask changes.
- `papr_battery`, `papr_sensors`, `papr_alarms` — battery state, environmental
  sensors, alarm latching/rendering.
- `papr_hal` — vendor-agnostic hardware contract; only exposes raw I²C
  transport (`papr_hal_i2c_write` / `papr_hal_i2c_read`) — sensor protocols
  live in portable drivers.

## SDP810-500Pa wiring (4-pin tube version)

| SDP810 pin | Signal | Connect to                              |
| ---------- | ------ | --------------------------------------- |
| 1          | VDD    | 3.3 V rail                              |
| 2          | SDA    | MCU SDA + 4.7 kΩ pull-up to 3.3 V       |
| 3          | GND    | board ground                            |
| 4          | SCL    | MCU SCL + 4.7 kΩ pull-up to 3.3 V       |

The "+" port (high-pressure tube) is plumbed across the device of interest
(filter inlet / mask-side breathing circuit); the "−" port goes to ambient
or the reference branch. I²C address is fixed at `0x25` for the SDP810 and
defined as `PAPR_SDP810_I2C_ADDR` in `papr_config.h`.

## GDY1124 absolute pressure sensor (rev 5)

The GigaDevice GDY1124 sits on the same I²C bus as the SDP810 at address
`0x76` (`PAPR_GDY1124_I2C_ADDR`). The driver:

1. Verifies the chip-ID register on init.
2. Reads the 24-byte calibration block (`T1..T3`, `P1..P9`).
3. Configures `CTRL_MEAS` for normal mode, ×4 temperature oversampling,
   ×16 pressure oversampling, and `CONFIG` for an IIR coefficient of 4
   with a 250 ms standby.
4. Per cycle, reads 6 raw bytes (24-bit pressure + 24-bit temperature)
   and applies the standard 64-bit fixed-point compensation algorithm.

Verify the chip-ID byte and register layout against the official GDY1124
datasheet — the constants are isolated in `papr_gdy1124.h` so no source
changes are needed if your part has a different ID or register map.

## BLE wireless control (rev 5)

The GD32VW553-UNIFI-EMH7 module sits on USART0 (`PA9` TX / `PA10` RX,
115200 8N1). Two extra GPIOs control the module:

| Signal       | GD32E517 pin | Notes                           |
| ------------ | ------------ | ------------------------------- |
| BLE_RESET_N  | PA11         | Active-low reset, push-pull     |
| BLE_HOST_WAKE| PA12         | Module → MCU wake notification  |

The host MCU exchanges length-prefixed binary frames with the module:

```
+------+------+------+------+--------------+------+
| 0xAA | 0x55 | LEN  | CMD  | PAYLOAD (LEN)| CRC8 |
+------+------+------+------+--------------+------+
```

CRC-8 is the same Sensirion polynomial (`0x31`, init `0xFF`) we already
use for the SDP810 — one helper function services both protocols.

Mobile → device commands:

| Code | Name           | Payload                               |
| ---- | -------------- | ------------------------------------- |
| 0x01 | SET_LEVEL      | `[u8 level]` (0=lo, 1=med, 2=hi)      |
| 0x02 | POWER_ON       | empty                                 |
| 0x03 | POWER_OFF      | empty                                 |
| 0x04 | MUTE_ALARM     | `[u16 LE seconds]`                    |
| 0x05 | RESET_FAULT    | empty                                 |
| 0x06 | GET_VERSION    | empty                                 |
| 0x07 | GET_TELEMETRY  | empty                                 |

Device → mobile notifications:

| Code | Name           | Payload                               |
| ---- | -------------- | ------------------------------------- |
| 0x80 | TELEMETRY      | packed 28-byte telemetry struct       |
| 0x81 | ACK            | `[u8 cmd]`                            |
| 0x82 | NACK           | `[u8 cmd, u8 reason]`                 |
| 0x83 | EVENT          | `[u8 event_type, …]`                  |

Telemetry is broadcast every `PAPR_BLE_TELEM_PERIOD_MS` (500 ms by
default) and on every state, level, or alarm-mask change.

## Safety notes

This is a reference design, **not** a certified medical device firmware. Any
real-world deployment requires:

- IEC 62304 software lifecycle compliance.
- Independent watchdog (FWDGT, IRC40K) with a separate clock domain — wired
  in revision 3.
- Redundant flow sensing or motor-current backup for the low-flow alarm.
- Validated battery fuel-gauge IC instead of pure voltage estimation.
- Confirmation that the L6235 thermal package and external `R_sense`
  dissipation match the target ambient and duty cycle.
- Field testing against the applicable PAPR standard (EN 12941, NIOSH 42 CFR 84).
