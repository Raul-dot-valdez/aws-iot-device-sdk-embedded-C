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
- **Revision 6** — adds two new modules:
  - **`papr_energy`** adaptive comfort / battery-saving algorithm. Detects
    breathing rate and intensity from the SDP810 dP stream and, when auto
    mode is enabled, nudges the blower setpoint up or down within the
    user-allowed range without ever falling below the safety floor. Also
    runs a coulomb counter for runtime estimation and a per-shift EWMA
    that learns each worker's typical session length across power cycles.
  - **`papr_keypad`** portable 3x3 switch-matrix scanner (POWER,
    LEVEL_UP/DOWN, MUTE, MODE, PAIR, BRIGHT, INFO, RESET) with debounce,
    press / release / long-press edges, and an event FIFO. POWER requires
    a long-press to actually start or stop the blower, preventing
    accidental power-off while wearing the hood.

  All revision-1 modules and headers remain in place; revisions 2-6 only
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
- `papr_keypad` — portable 3x3 switch-matrix scanner. Drives one row at a
  time, samples columns, debounces, emits PRESS / RELEASE / LONG_PRESS
  events into a FIFO consumed by the controller. Layout:
  `[POWER | LEVEL_UP | LEVEL_DOWN] / [MUTE | MODE | PAIR] /
   [BRIGHT | INFO | RESET]`.
- `papr_energy` — adaptive comfort and runtime estimator. Tracks dP for
  breathing rate / amplitude, recommends a flow level for auto mode,
  runs a coulomb counter for "minutes remaining", and smooths the
  worker's session lengths with an EWMA.
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
| 0x08 | SET_AUTO_MODE  | `[u8 enable]` (0=manual, 1=adaptive)  |

Device → mobile notifications:

| Code | Name           | Payload                               |
| ---- | -------------- | ------------------------------------- |
| 0x80 | TELEMETRY      | packed 28-byte telemetry struct       |
| 0x81 | ACK            | `[u8 cmd]`                            |
| 0x82 | NACK           | `[u8 cmd, u8 reason]`                 |
| 0x83 | EVENT          | `[u8 event_type, …]`                  |

Telemetry is broadcast every `PAPR_BLE_TELEM_PERIOD_MS` (500 ms by
default) and on every state, level, or alarm-mask change. The telemetry
payload is 29 bytes (see the byte map in the Mobile App Reference below)
and includes the rev-6 adaptive-comfort fields (`breaths_per_min`,
`remaining_minutes`, `auto_mode_active`).

## Switch matrix (rev 6)

The 3x3 keypad replaces the earlier two-button input. On the GD32E517RE
port it consumes 6 GPIOs that were previously unallocated:

| Function   | GD32E517 pin | Direction          |
| ---------- | ------------ | ------------------ |
| ROW0..ROW2 | PB3 / PB4 / PB5  | open-drain output |
| COL0..COL2 | PB8 / PB9 / PB11 | input + pull-up   |

Key matrix layout:

```
   col 0       col 1       col 2
  [POWER]    [LVL UP]    [LVL DN]    row 0
  [MUTE ]    [MODE  ]    [PAIR  ]    row 1
  [BRIGHT]   [INFO  ]    [RESET ]    row 2
```

Semantics:

- **POWER** — long-press (≥ 1.2 s) starts the blower from standby or
  initiates shutdown from running / alarm. Short press has no effect, so
  brushing the hood against the user's body cannot power the unit off.
- **LEVEL_UP / LEVEL_DOWN** — bump the manual flow level by one step.
- **MUTE** — silence the alarm buzzer for 60 s.
- **MODE** — toggle adaptive-comfort (auto level) on or off.
- **PAIR** — reset the BLE module so the mobile app can re-pair.
- **RESET** — clear a latched fault and re-arm the supervisor.
- **BRIGHT / INFO** — reserved for the display HMI in a later revision.

## Adaptive comfort algorithm (rev 6)

When the user enables auto mode (MODE key or BLE `SET_AUTO_MODE` 1):

1. The energy module samples dP from the SDP810 every 100 ms.
2. A 32-sample moving average centres the signal; threshold-crossings of
   the centred signal count as inhale onsets and produce a breaths-per-
   minute estimate (EWMA smoothed).
3. A peak-amplitude tracker estimates breath intensity in Pa.
4. Every 30 s (hold-off period), the algorithm may bump the level down by
   one step if the worker is breathing lightly (≤ 12 bpm and < 40 Pa
   amplitude) or up by one step if they are breathing heavily (≥ 24 bpm
   or > 80 Pa amplitude). The floor is always `PAPR_LEVEL_LOW`.
5. In parallel, a coulomb counter integrates pack current to estimate
   minutes of runtime remaining, and an EWMA tracks the user's typical
   session length so the mobile app can warn early when the budget runs
   short of their usual shift duration.

## Mobile App Reference (rev 7)

This section is written for the **app developer**. It specifies every screen,
the data each screen reads, the commands each control sends, and the end-to-end
use-case flows — all expressed in terms of the BLE protocol the firmware
already implements (see "BLE wireless control" above). Nothing here changes the
firmware; it is the contract the app must follow to interoperate seamlessly.

### Transport assumptions

The GD32VW553-UNIFI-EMH7 module is expected to expose a **Nordic UART Service
(NUS)-style transparent serial bridge** over GATT:

```
  Service  UUID  6E400001-B5A3-F393-E0A9-E50E24DCCA9E   (PAPR link)
   * RX char 6E400002-...   WRITE / WRITE_NO_RESPONSE   app -> firmware
   * TX char 6E400003-...   NOTIFY                      firmware -> app
```

Whatever bytes the app writes to **RX** arrive at the firmware UART; whatever
the firmware emits is delivered as **TX** notifications. The app therefore
builds and parses the *exact* frames documented below:

```
  +------+------+------+------+------------------+------+
  | 0xAA | 0x55 | LEN  | CMD  | PAYLOAD (LEN B)  | CRC8 |
  +------+------+------+------+------------------+------+
   sync0  sync1  len    cmd      len bytes        crc

  CRC8 = Sensirion CRC-8 over [LEN, CMD, PAYLOAD]
         polynomial 0x31, init 0xFF, no final XOR, no reflection.
```

A reference CRC-8 for the app (any language) — process LEN, CMD, then each
payload byte:

```
  crc = 0xFF
  for b in (LEN, CMD, *PAYLOAD):
      crc ^= b
      repeat 8 times:
          crc = ((crc << 1) ^ 0x31) & 0xFF  if (crc & 0x80) else (crc << 1) & 0xFF
```

> A single BLE notification may not align to one frame. The app MUST run a
> byte-oriented parser (resync on `AA 55`) exactly like the firmware's RX state
> machine — never assume one notification == one frame.

### Decoding helpers (apply to telemetry fields)

```
  state          0 INIT  1 SELF_TEST  2 STANDBY  3 RUNNING
                 4 ALARM  5 SHUTDOWN   6 FAULT
  level          0 LOW (170 L/min)  1 MED (195)  2 HIGH (220)
  alarms (bits)  0x01 LOW_FLOW   0x02 LOW_BATTERY  0x04 CRIT_BATTERY
                 0x08 FILTER_CLOG 0x10 MOTOR_FAULT 0x20 SENSOR_FAULT
                 0x40 OVERTEMP
  temperature    temperature_c10 / 10        -> degrees C
  diff pressure  pressure_pa                 -> Pa (signed)
  abs pressure   absolute_pressure_pa / 100  -> hPa
  battery volts  battery_mv / 1000           -> V
  duty           duty_permille / 10          -> %
```

### Screen map (navigation)

```
                         +------------------+
                         |   1. CONNECT     |
                         |   (scan & pair)  |
                         +--------+---------+
                                  | link up + first TELEMETRY
                                  v
                         +------------------+
              +--------->|   2. HOME        |<---------+
              |          |   (dashboard)    |          |
              |          +--+----+----+--+--+          |
              |             |    |    |  |             |
       tab bar|       +-----+    |    |  +-----+       |tab bar
              |       v          v    v        v       |
        +-----+--+ +--+-----+ +--+---++ +------++--+ +-+------+
        |3.CTRL  | |4.AUTO  | |5.BATT | |6.ALARMS | |7.DIAG  |
        |manual  | |comfort | |runtime| |/events  | |detail  |
        +--------+ +--------+ +-------+ +---------+ +--------+
                                  |
                                  v
                         +------------------+
                         |   8. SETTINGS    |
                         |   / about        |
                         +------------------+

  A FAULT event (or state == FAULT) pops a modal over any screen
  (see use case "Fault handling").
```

### 1. Connect / pair

```
  +--------------------------------------+
  |  PAPR Control            [ gear ]    |
  +--------------------------------------+
  |                                      |
  |   Looking for respirators...   (o)   |
  |                                      |
  |   +------------------------------+   |
  |   |  PAPR-7F3A      RSSI -52 dBm |   |
  |   |  [   CONNECT   ]             |   |
  |   +------------------------------+   |
  |   +------------------------------+   |
  |   |  PAPR-1C90      RSSI -77 dBm |   |
  |   |  [   CONNECT   ]             |   |
  |   +------------------------------+   |
  |                                      |
  |   ( ) scan again                     |
  +--------------------------------------+

  Behaviour:
   - Scan/filter on the NUS service UUID.
   - On CONNECT: subscribe to TX notifications, then send GET_VERSION (0x06)
     and GET_TELEMETRY (0x07).
   - Advance to HOME when the first TELEMETRY (0x80) arrives.
   - ACK to GET_VERSION carries [major, minor, patch]; show it on Settings.
```

### 2. Home (dashboard)

```
  +--------------------------------------+
  |  PAPR-7F3A            * connected    |
  +--------------------------------------+
  |   STATE: RUNNING        AUTO: OFF    |   <- state, auto_mode_active
  |                                      |
  |        +----------------------+      |
  |        |     195 L/min        |      |   <- flow_lpm
  |        |    level: MED        |      |   <- level
  |        +----------------------+      |
  |                                      |
  |   Battery  [#######---] 72%   3:24   |   <- soc, remaining_minutes
  |   Breathing      18 / min            |   <- breaths_per_min
  |   Filter dP      142 Pa              |   <- pressure_pa
  |                                      |
  |   +--------+ +--------+ +---------+  |
  |   | POWER  | | LEVEL  | |  MUTE   |  |
  |   |  OFF   | |  MED   | | (alarm) |  |
  |   +--------+ +--------+ +---------+  |
  +--[ HOME ][CTRL][AUTO][BATT][ALRM ]--+

  Reads : every TELEMETRY notification.
  POWER : POWER_ON (0x02) / POWER_OFF (0x03).
  Quick controls mirror the Manual Control screen.
  Banner turns amber/red when alarms != 0.
```

### 3. Manual control

```
  +--------------------------------------+
  |  < Home            Manual Control    |
  +--------------------------------------+
  |                                      |
  |   Power            [  ON  ][  OFF ]  |   -> POWER_ON / POWER_OFF
  |                                      |
  |   Flow level                         |
  |     ( ) LOW    170 L/min             |   -> SET_LEVEL 0
  |     (o) MED    195 L/min             |   -> SET_LEVEL 1
  |     ( ) HIGH   220 L/min             |   -> SET_LEVEL 2
  |                                      |
  |   Adaptive comfort   [  OFF |  ON ]  |   -> SET_AUTO_MODE 0/1
  |                                      |
  |   ! Changing level is ignored while  |
  |     auto mode is ON (firmware owns   |
  |     the setpoint).                   |
  +--------------------------------------+

  Each control waits for ACK (0x81) before settling its UI state; on NACK
  (0x82) it reverts and shows the reason (see Error handling).
  LEVEL radio is also updated by EVENT LEVEL_CHANGE (auto-mode steps).
```

### 4. Adaptive comfort (energy saving)

```
  +--------------------------------------+
  |  < Home          Adaptive Comfort    |
  +--------------------------------------+
  |   Auto mode          [ OFF | *ON* ]  |   -> SET_AUTO_MODE 1
  |                                      |
  |   Firmware is matching flow to your  |
  |   breathing to save battery.         |
  |                                      |
  |   Breathing rate     18 / min        |   <- breaths_per_min
  |   Current level      MED  (auto)     |   <- level (driven by firmware)
  |                                      |
  |   Est. runtime now   3 h 24 m        |   <- remaining_minutes
  |   Est. at HIGH       2 h 05 m        |   (app-side projection, optional)
  |                                      |
  |   [######------] light  <->  heavy   |   <- gauge from breaths_per_min
  +--------------------------------------+

  In auto mode the app must treat `level` as read-only and reflect
  LEVEL_CHANGE events; SET_LEVEL writes are pointless (firmware owns it).
```

### 5. Battery & runtime

```
  +--------------------------------------+
  |  < Home            Battery & Power   |
  +--------------------------------------+
  |        +----------------------+      |
  |        |        72 %          |      |   <- battery_soc_percent
  |        |   [#######---]       |      |
  |        +----------------------+      |
  |                                      |
  |   Pack voltage     14.8 V            |   <- battery_mv / 1000
  |   Draw             1.92 A            |   <- battery_ma / 1000
  |   Runtime left     3 h 24 m          |   <- remaining_minutes
  |                                      |
  |   Low-battery warning at  ~13.2 V    |   (informational)
  |   Critical / cutoff       ~12.0 V    |
  +--------------------------------------+

  When alarms & (LOW_BATTERY|CRIT_BATTERY) the runtime figure turns red.
```

### 6. Alarms / events

```
  +--------------------------------------+
  |  < Home               Alarms         |
  +--------------------------------------+
  |  ACTIVE                              |   <- decode alarms bitmask
  |   [!] LOW FLOW                       |
  |   [!] FILTER CLOGGED                 |
  |                                      |
  |   [   MUTE BUZZER 60s   ]            |   -> MUTE_ALARM (60)
  |                                      |
  |  HISTORY (app-side log of EVENTs)    |
  |   12:04  alarm  FILTER_CLOG set      |
  |   11:58  state  RUNNING              |
  |   11:58  level  MED                  |
  +--------------------------------------+

  Sources:
   - Live set: TELEMETRY.alarms bitmask.
   - Transitions: EVENT (0x83) ALARM_CHANGE carries [u32 LE mask].
   - MUTE silences the buzzer only; the alarm stays active until the
     condition clears (firmware keeps airflow on through alarms).
```

### 7. Diagnostics (detail)

```
  +--------------------------------------+
  |  < Home            Diagnostics       |
  +--------------------------------------+
  |  Flow            195 L/min           |  flow_lpm
  |  Filter dP       142 Pa              |  pressure_pa (signed)
  |  Ambient         1006 hPa            |  absolute_pressure_pa / 100
  |  Motor temp      35.0 C              |  temperature_c10 / 10
  |  Motor speed     21800 rpm           |  motor_rpm
  |  Driver demand   61.0 %              |  duty_permille / 10
  |  Breathing       18 /min             |  breaths_per_min
  |  State           RUNNING (3)         |  state
  |  Alarms          0x09                |  alarms (raw hex)
  |  FW version      0.6.0               |  GET_VERSION ACK payload
  |                                      |
  |  [  CLEAR FAULT  ]   (state==FAULT)  |  -> RESET_FAULT (0x05)
  +--------------------------------------+
```

### 8. Settings / about

```
  +--------------------------------------+
  |  < Home               Settings       |
  +--------------------------------------+
  |  Device         PAPR-7F3A            |
  |  Firmware       v0.6.0               |   <- GET_VERSION
  |  Telemetry rate 500 ms (read-only)   |
  |                                      |
  |  [   RE-PAIR BLE MODULE   ]          |   (BLE PAIR key is on the unit;
  |                                      |    app side just disconnects and
  |                                      |    re-scans)
  |  [   FORGET DEVICE        ]          |
  |  [   CLEAR FAULT          ]          |   -> RESET_FAULT (0x05)
  +--------------------------------------+
```

### Telemetry payload byte map (CMD 0x80, LEN = 29)

All multi-byte integers are little-endian.

```
  off  size  field                  decode
  ---  ----  ---------------------  -----------------------------
   0    1    state                  enum 0..6
   1    1    level                  enum 0..2
   2    4    alarms                 u32 bitmask
   6    2    flow_lpm               u16  L/min
   8    2    pressure_pa            i16  Pa (filter dP, signed)
  10    4    absolute_pressure_pa   u32  Pa ( /100 = hPa )
  14    2    temperature_c10        i16  0.1 C
  16    2    battery_mv             u16  mV
  18    2    battery_ma             u16  mA
  20    1    battery_soc_percent    u8   0..100
  21    2    motor_rpm              u16  rpm
  23    2    duty_permille          u16  0.1 %
  25    1    breaths_per_min        u8
  26    2    remaining_minutes      u16  minutes
  28    1    auto_mode_active       u8   0/1
  ---  ----
   29  total payload bytes
```

### Command / response reference

```
  CMD   name            payload (app -> fw)     expected response
  ----  --------------  ----------------------  -----------------------
  0x01  SET_LEVEL       [u8 level 0..2]         ACK 0x81 [0x01]
  0x02  POWER_ON        (none)                  ACK 0x81 [0x02]
  0x03  POWER_OFF       (none)                  ACK 0x81 [0x03]
  0x04  MUTE_ALARM      [u16 LE seconds]        ACK 0x81 [0x04]
  0x05  RESET_FAULT     (none)                  ACK 0x81 [0x05]
  0x06  GET_VERSION     (none)                  ACK 0x81 [maj,min,pat]
  0x07  GET_TELEMETRY   (none)                  TELEMETRY 0x80 [..29..]
  0x08  SET_AUTO_MODE   [u8 0|1]                ACK 0x81 [0x08]

  Unsolicited from firmware:
  0x80  TELEMETRY       [29 bytes]              every 500 ms + on change
  0x83  EVENT           [u8 type, ...]          on transition
          type 0x01 STATE_CHANGE  [u8 state]
          type 0x02 ALARM_CHANGE  [u32 LE mask]
          type 0x03 LEVEL_CHANGE  [u8 level]
          type 0x04 FAULT         [..]

  Errors:
  0x82  NACK            [u8 cmd, u8 reason]
          reason 1 = bad/!len payload   2 = unknown cmd   3 = CRC error
```

### Use case A — connect and start

```
  app                         firmware (via module)
   |  scan, connect, subscribe TX    |
   |-------------------------------->|
   |  WRITE GET_VERSION (0x06)       |
   |-------------------------------->|
   |        ACK [0,6,0]              |
   |<--------------------------------|
   |  WRITE GET_TELEMETRY (0x07)     |
   |-------------------------------->|
   |        TELEMETRY (state=STANDBY)|
   |<--------------------------------|
   |  user taps POWER ON             |
   |  WRITE POWER_ON (0x02)          |
   |-------------------------------->|
   |        ACK [0x02]               |
   |<--------------------------------|
   |        EVENT STATE_CHANGE(RUN)  |
   |<--------------------------------|
   |        TELEMETRY (state=RUNNING)|
   |<--------------------------------|  (then every 500 ms)
```

### Use case B — enable adaptive comfort

```
  app                         firmware
   |  toggle AUTO -> ON              |
   |  WRITE SET_AUTO_MODE [1] (0x08) |
   |-------------------------------->|
   |        ACK [0x08]               |
   |<--------------------------------|
   |  ...breathing light for 30 s... |
   |        EVENT LEVEL_CHANGE [LOW] |   firmware steps down to save power
   |<--------------------------------|
   |        TELEMETRY (level=LOW,    |
   |          auto_mode_active=1)    |
   |<--------------------------------|
   |  UI: lock the level selector,   |
   |      show "auto", update gauge  |
```

### Use case C — alarm and mute

```
  app                         firmware
   |        EVENT ALARM_CHANGE       |
   |          mask=0x09 (LOWFLOW|CLOG)
   |<--------------------------------|
   |  banner red, open Alarms screen |
   |  user taps MUTE 60s             |
   |  WRITE MUTE_ALARM [60] (0x04)   |
   |-------------------------------->|
   |        ACK [0x04]               |
   |<--------------------------------|
   |  buzzer silent; alarm still set |
   |  until condition clears, then:  |
   |        EVENT ALARM_CHANGE mask=0|
   |<--------------------------------|
```

### Use case D — fault handling

```
  app                         firmware
   |        EVENT FAULT / STATE=FAULT|
   |<--------------------------------|
   |  modal: "Device fault - air     |
   |  flow stopped. Remove hood and  |
   |  check unit."                   |
   |  user taps CLEAR FAULT          |
   |  WRITE RESET_FAULT (0x05)       |
   |-------------------------------->|
   |        ACK [0x05]               |
   |<--------------------------------|
   |        EVENT STATE_CHANGE       |
   |          (INIT -> SELF_TEST ...)|
   |<--------------------------------|

  RESET_FAULT only acts when state == FAULT; otherwise it is a harmless
  no-op that still returns ACK.
```

### Robustness rules for the app

```
  - Resync the frame parser on AA 55; tolerate split/coalesced notifications.
  - Verify CRC-8 on every received frame; drop on mismatch (do not crash).
  - Treat TELEMETRY as the source of truth; reconcile optimistic UI on ACK.
  - If no TELEMETRY for > 3 s, show "reconnecting" and re-issue GET_TELEMETRY.
  - On BLE drop, auto-reconnect, re-subscribe, GET_VERSION + GET_TELEMETRY.
  - Never block the safety path: the unit runs and alarms locally with or
    without a phone connected. The app is an accessory, not a controller of
    last resort.
```

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
