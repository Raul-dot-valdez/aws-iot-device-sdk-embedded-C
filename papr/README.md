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
- **Revision 7** — **OTA firmware update** (`papr_ota`) plus the supporting
  mobile-app flow. The app streams a new image over BLE into the inactive
  flash slot; the firmware verifies it with CRC-32, then sets a boot flag and
  resets so a bootloader runs the new image (with rollback). The running
  version is reported over BLE so users can check whether they are up to date.
  Updates are accepted **only while the unit is idle (STANDBY)** — never while
  the worker is breathing through it.
- **Revision 8** — **Design-for-Manufacture / Design-for-Test** support for
  mass production with an end-of-line flashing/test station:
  - **`papr_provision`** stores per-unit identity and calibration (serial,
    hardware revision, manufacture date, BLE MAC, flow/current calibration)
    in a dedicated flash page that survives OTA updates. The station writes
    it; the application reads it at boot.
  - **`papr_factory`** is a built-in self-test (BIST) + factory command loop.
    The station asserts a TEST_MODE pad and resets; the firmware then runs
    BIST over every subsystem and exposes provisioning / actuation / sensor
    commands over the UART test pads (same framing as BLE). See "Production
    test" below.
- **Revision 9** — **secure boot, dual-bank flash, and cybersecurity** aligned
  to the stringent connected-device regimes (ETSI EN 303 645, EN 18031,
  NIST IR 8259/8425, IEC 62443-4-2, FDA premarket cybersecurity):
  - **`papr_sha256`** self-contained SHA-256 + HMAC-SHA256 (known-answer
    self-tested at boot).
  - **`papr_secure`** authenticated control (a BLE central must pass an
    HMAC-SHA256 challenge/response over the per-device key before any
    state-changing command is honoured), signed + anti-rollback OTA
    (SHA-256 image hash + vendor signature + monotonic security version),
    audit counters, and auth lockout.
  - A standalone **reference bootloader** (`boot/papr_boot.c`) verifies each
    slot's signed manifest before running it (secure boot) and implements
    one-try rollback across two app slots. Dual-bank linker scripts
    (`linker/`) and CMake options build either slot plus the bootloader.
    The firmware version is centralised in `papr_version.h` (currently
    **v0.9.0**).

  All revision-1 modules and headers remain in place; revisions 2-9 only
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
- `papr_ota` — secure over-the-air update receiver. Streams the image into the
  inactive flash slot while hashing it (SHA-256), then verifies integrity
  (hash), authenticity (vendor signature over the manifest) and anti-rollback
  (monotonic security version) before commit. Gated so a transfer can only
  begin while the unit is idle.
- `papr_provision` — per-unit identity + calibration in a dedicated flash
  page (serial, HW rev, mfg date, BLE MAC, flow/current cal). Written by the
  production station, read by the application; survives OTA updates.
- `papr_factory` — end-of-line built-in self-test (BIST) and factory command
  loop. Entered at boot when the TEST_MODE pad is asserted; talks the same
  framed protocol as BLE over the UART test pads.
- `papr_sha256` — self-contained SHA-256 + HMAC-SHA256 with a known-answer
  self-test; underpins OTA integrity, image authenticity, and BLE auth.
- `papr_secure` — cybersecurity services: authenticated control (challenge /
  response session auth), signed + anti-rollback image verification, audit
  counters, and auth lockout. See "Cybersecurity" below.
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
| 0x09 | OTA_BEGIN      | signed manifest (108 B, see OTA below)|
| 0x0A | OTA_DATA       | `[u32 offset][image bytes…]`          |
| 0x0B | OTA_END        | empty (verify hash + signature)       |
| 0x0C | OTA_APPLY      | empty (commit + reboot)               |
| 0x0D | OTA_ABORT      | empty                                 |
| 0x0E | OTA_STATUS     | empty (request status)                |
| 0x10 | AUTH_BEGIN     | empty -> AUTH_CHALLENGE               |
| 0x11 | AUTH_RESPONSE  | `[u8 tag[16]]` (HMAC over the nonce)  |
| 0x12 | SEC_STATUS     | empty (request security status)       |

State-changing commands (SET_LEVEL, POWER_*, MUTE_ALARM, RESET_FAULT,
SET_AUTO_MODE, all OTA_*) require an **authenticated session** — see
"Cybersecurity" below. Read-only commands and the AUTH handshake do not.

Device → mobile notifications:

| Code | Name           | Payload                               |
| ---- | -------------- | ------------------------------------- |
| 0x80 | TELEMETRY      | packed 29-byte telemetry struct       |
| 0x81 | ACK            | `[u8 cmd]`                            |
| 0x82 | NACK           | `[u8 cmd, u8 reason]`                 |
| 0x83 | EVENT          | `[u8 event_type, …]`                  |
| 0x84 | OTA_STATUS     | `[u8 state, u8 error, u8 pct, u8 maj,min,pat]` |
| 0x85 | AUTH_CHALLENGE | `[u8 nonce[16], u32 counter]`         |
| 0x86 | SEC_STATUS     | `[u8 auth_state, u8 flags, u16 fail, u16 reject]` |

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

## OTA firmware update (rev 7, secured in rev 9)

Workers (or a supervisor via the mobile app) can check the installed firmware
version and update it wirelessly. The design favours safety, authenticity, and
recoverability over speed. From rev 9 every image is cryptographically
verified (see "Cybersecurity").

### Flash map (GD32E517RE, 512 KB) — final rev-9 layout

```
  0x08000000  +-------------------+  bootloader (28 KB): verifies + selects a
              | bootloader        |   slot, one-try rollback
  0x08007000  +-------------------+  boot-state page (mutable lifecycle)
  0x08007800  +-------------------+  provisioning page (serial/cal/keys)
  0x08008000  +-------------------+  slot A app (238 KB) + 2 KB signed
              | slot A            |   manifest trailer @ 0x08043800
  0x08044000  +-------------------+  slot B app (238 KB) + 2 KB signed
              | slot B            |   manifest trailer @ 0x0807F800
  0x08080000  +-------------------+
```

The HAL stages the image into whichever slot is **not** running (chosen from
`SCB->VTOR`). The slot trailer holds the signed manifest
(`papr_img_manifest_t`: magic, size, sec_version, fw version, SHA-256,
signature). `papr_hal_ota_commit()` writes it and marks the slot PENDING in the
boot-state page; the bootloader re-verifies it before booting.

### Update flow & safety

```
  OTA_BEGIN  --> papr_ota_begin    (idle? size ok? not a downgrade?) erase slot
  OTA_DATA*  --> papr_ota_write    sequential word-aligned chunks -> flash,
                                   hashed (SHA-256) as they land
  OTA_END    --> papr_ota_finish   finalize hash; verify integrity + signature
                                   + anti-rollback
  OTA_APPLY  --> papr_ota_apply    bump sec-version, write signed manifest, reset
  OTA_ABORT  --> papr_ota_abort    back to IDLE at any time
```

- **Authenticated:** OTA commands require an authenticated BLE session.
- **Authentic + intact:** the streamed bytes must hash to the manifest
  SHA-256 and the manifest signature must verify against the vendor key
  (`PAPR_SEC_REQUIRE_SIGNED_OTA`, default on). A tampered image is refused.
- **Idle-only:** `OTA_BEGIN` is rejected unless the supervisor is in STANDBY,
  and the blower cannot be started while a transfer is in progress.
- **Anti-rollback:** an image whose `sec_version` is below the stored
  monotonic floor is rejected.
- **Recoverable:** the active slot is never erased — only the inactive slot —
  so a failed/interrupted update cannot brick the device; the bootloader
  rolls back if the new image fails to boot and confirm.

### Manifest (OTA_BEGIN payload, 108 bytes)

```
  off  size  field         notes
  ---  ----  ------------  -------------------------------------------------
   0    4    image_size    bytes of the app image (excl. trailer), u32 LE
   4    4    sec_version   anti-rollback monotonic counter, u32 LE
   8    1    fw_major
   9    1    fw_minor
  10    1    fw_patch
  11    1    sig_len       signature length (32 for the HMAC reference)
  12   32    sha256        SHA-256 of the image bytes
  44   64    signature     vendor signature over the manifest header+hash
  --- 108    total
  (magic is set by the device; the signature covers the 48-byte header
   ending at the signature field, i.e. magic..sha256.)
```

### OTA error codes (in OTA_STATUS.error)

```
  0 NONE     1 BUSY (not idle)   2 SIZE (too big)   3 SEQUENCE (bad offset)
  4 ALIGN    5 FLASH             6 HASH mismatch     7 AUTH (bad signature)
  8 ROLLBACK 9 STATE
  OTA states: 0 IDLE  1 RECEIVING  2 READY  3 ERROR
```

## Cybersecurity (rev 9)

The firmware ships **secure by default** (enforcement on unless a build flag
disables it) and maps to the controls common to the stringent connected-device
regimes:

```
  Control                          Standard reference
  -------------------------------  ----------------------------------------
  No default passwords / unique    ETSI EN 303 645 5.1; EN 18031; NIST IR
   per-device credential            8425; UK PSTI
  Authenticated, least-privilege   ETSI 5.5; IEC 62443-4-2 CR1/CR2; FDA
   control access                   "authentication / authorization"
  Secure software updates +        ETSI 5.3; EN 18031; NIST IR 8259 ;
   integrity & authenticity         FDA "integrity / authenticity"
  Anti-rollback                    FDA premarket; IEC 62443 CR3.4
  Secure boot / verified boot      FDA; IEC 62443 CR3.4
  Protect security parameters      ETSI 5.4 (keys in protected storage,
   (keys) / no key exfiltration     debug lock after production)
  Minimise attack surface          ETSI 5.6 (telemetry read-only; control
                                    gated; bounded, CRC'd parsers)
  Examine/secure-by-default +      ETSI 5.12 / 5.7; NIST logging
   audit / event logging
```

### Authenticated control (challenge / response)

Telemetry (read) is open; every **state-changing** command is gated behind a
session the central must authenticate, proving knowledge of the per-device
session key without transmitting it:

```
  app                                   device
   |  AUTH_BEGIN                         |
   |----------------------------------->|  rng() nonce; counter++
   |  AUTH_CHALLENGE [nonce16, counter] |
   |<-----------------------------------|
   |  tag = HMAC-SHA256(session_key,    |
   |        nonce || counter_le)[0:16]  |
   |  AUTH_RESPONSE [tag16]             |
   |----------------------------------->|  constant-time compare
   |  ACK  (session now OPEN)            |
   |<-----------------------------------|
   |  ... control commands allowed ...   |
```

- The session key is provisioned per device (`PAPR_KEY_SESSION`) and shared
  with the authorized app via the user's account / pairing — there is no
  universal default credential.
- The nonce is random per challenge and the counter monotonic, so a captured
  response cannot be replayed.
- After `PAPR_SEC_AUTH_MAX_FAILS` (5) bad responses the device locks out new
  challenges for `PAPR_SEC_AUTH_LOCKOUT_MS` (30 s). Failures and OTA rejects
  are counted and exposed via `SEC_STATUS` for audit.
- A control command on an unauthenticated link is answered with NACK reason 5
  (auth required). On BLE disconnect / re-pair the session is closed.

`SEC_STATUS` flags: `0x01` crypto-self-test-ok, `0x02` auth-required,
`0x04` signed-OTA-required, `0x08` debug-locked.

### Keys & secrets

```
  PAPR_KEY_SESSION  per-device secret (HMAC) for BLE session auth
  PAPR_KEY_VENDOR   vendor key used to verify the OTA / boot image signature
```

Keys are provisioned into the protected provisioning page at the factory and
the page is read-protected when `papr_hal_secure_lock_debug()` runs at
end-of-line. The reference HAL verifies signatures with HMAC-SHA256 (a real,
testable MAC); a production port replaces `papr_hal_sec_verify()` with
ECDSA-P256 / Ed25519 on the MCU crypto engine so the signing key never leaves
the vendor's HSM and only a **public** key sits on the device.

### Crypto self-test

`papr_secure_init()` runs `papr_sha256_selftest()` (NIST known-answer vectors
for SHA-256 and HMAC-SHA256) at boot; if it fails the device refuses all
security operations (no auth, no OTA) — a tamper / corruption tripwire.

## Secure boot & dual-bank (rev 9)

A standalone bootloader establishes the root of trust and gives the OTA path
its rollback safety net.

```
  reset
    |
    v
  bootloader (0x08000000)
    |  SHA-256 self-test (refuse if it fails)
    |  for each slot: verify manifest (image hash + vendor signature)
    |  a slot left TRYING but never confirmed -> mark INVALID (rollback)
    |  pick: a verifiable PENDING image (one try) else the highest
    |        sec_version VALID image
    |  set VTOR + MSP, jump to the chosen slot
    v
  application
    |  on a healthy self-test: papr_hal_ota_confirm() -> slot VALID
    |  (a freshly-OTA'd image that crashes before this is rolled back)
```

- **Verified boot:** the bootloader recomputes the image SHA-256 and checks
  the vendor signature on every reset; an unsigned/tampered slot is refused
  and marked INVALID (`PAPR_BOOT_REQUIRE_SIG`, default on).
- **One-try rollback:** OTA marks the new slot PENDING; the bootloader marks it
  TRYING and boots it once; only an app self-test confirm promotes it to VALID.
  A boot loop therefore reverts to the last good image automatically.
- **No-brick:** the running slot is never erased during an update; if nothing
  verifies the bootloader stays in a recovery loop awaiting SWD/DFU reflash.

### Building the secure, dual-bank set

```sh
# bootloader
cmake -S papr -B papr/bl -DPAPR_TARGET=GD32E517RE \
      -DGD32E51X_SDK_DIR=... -DCMAKE_TOOLCHAIN_FILE=... -DPAPR_BUILD_BOOTLOADER=ON
cmake --build papr/bl --target papr_bootloader     # -> 0x08000000

# application linked for slot A (golden / factory image)
cmake -S papr -B papr/a  -DPAPR_TARGET=GD32E517RE -DPAPR_APP_SLOT=A \
      -DGD32E51X_SDK_DIR=... -DCMAKE_TOOLCHAIN_FILE=...
cmake --build papr/a                                # -> 0x08008000

# application linked for slot B (OTA target image)
cmake -S papr -B papr/b  -DPAPR_TARGET=GD32E517RE -DPAPR_APP_SLOT=B ...
```

After building an app image, the **release/signing tool** (off-device, holding
the private key) computes the SHA-256 over the image, builds the manifest, and
signs it; the manifest is appended to the slot trailer (production flashing) or
delivered with the image (OTA). Leaving `PAPR_APP_SLOT` unset keeps the
original single-image dev build (`hal/gd32e517re/gd32e517re.ld`) for
bring-up without a bootloader.

## Production test (DFM / DFT, rev 8)

Written for the **manufacturing / test engineer** building the end-of-line
(EOL) flashing test station. The firmware is designed so one fixture can
flash, self-test, calibrate, and serialise each unit, and log a structured
pass/fail record for traceability.

### Board access points (design these into the PCB)

```
  interface     pins (GD32E517RE)   fixture use
  ------------  ------------------  -------------------------------------------
  SWD           PA13 SWDIO          flash bootloader + golden app; debug;
                PA14 SWCLK          read protection (set after final test)
  UART pads     PA9  TX / PA10 RX   factory command protocol (BIST, provision,
                                    actuation) — pogo pads, shared with BLE
  TEST_MODE     PC12  (pull-up)     fixture pulls LOW + resets -> factory mode
  BOOT0 strap   BOOT0               recovery: ROM serial bootloader if SWD dead
  NRST          NRST                fixture-controlled reset
  power/ICT     VBAT, 3V3, GND      bench supply + rail measurement test points
```

Lay these out as accessible test pads / a debug connector on the panel, with
fiducials for the bed-of-nails. Keep TEST_MODE pulled up so a field unit never
boots into test.

### EOL station sequence

```
  1. Place panel on bed-of-nails; apply bench supply.
  2. SWD: flash bootloader @0x08000000 + golden app @0x08008000 (slot A).
        (CRC/verify the readback.)
  3. Assert TEST_MODE low, pulse NRST -> unit boots into papr_factory.
        Unit emits FCT_RSP_INFO unsolicited (booted-into-test handshake).
  4. FCT_RUN_BIST  -> read pass_mask vs executed_mask + measured values.
        Fixture also confirms LED/buzzer/BLE optically/acoustically/over-air.
  5. FCT_WRITE_PROV -> serial (laser-mark to match), hw_rev, mfg_date,
        BLE MAC, and the flow/current calibration the fixture just measured.
  6. FCT_READ_PROV  -> verify the record reads back valid (magic+CRC).
  7. FCT_REBOOT     -> unit resets into the application; final functional
        check (optional BLE link test from the station).
  8. SWD: enable flash read-protection / debug lock. Log the record.
```

### Flash map (with provisioning)

```
  0x08000000  bootloader        30 KB   validates a slot, rollback
  0x08007800  provisioning      2 KB    serial / cal / MAC (papr_provision)
  0x08008000  slot A (app)     240 KB   golden image flashed at production
  0x08044000  slot B (app)     240 KB   OTA staging in the field
  0x08080000  (end of 512 KB)
```

The provisioning page sits in the bootloader region, **outside both app
slots**, so a field OTA update never erases the unit's identity or
calibration.

### Built-in self-test (BIST) coverage

`papr_factory_run_bist()` fills a report with an `executed_mask`, a
`pass_mask`, and measured values for the station log:

```
  bit     subsystem   check
  ------  ----------  -------------------------------------------------------
  0x0001  PROVISION   provisioning page present + valid (warn on blank board)
  0x0002  BATTERY     pack/bench mV within [10000, 17500]
  0x0004  TEMP        NTC reads a sane -30..85 C
  0x0008  SDP810      continuous-mode frame with valid Sensirion CRC
  0x0010  GDY1124     chip-ID match + a successful compensated read
  0x0020  BLOWER      spins at test VREF, no L6235 DIAG fault
  0x0040  TACHO       RPM >= PAPR_FACTORY_BLOWER_MIN_RPM
  0x0080  FLOW        flow rises above PAPR_FACTORY_FLOW_MIN_LPM when spinning
  0x0100  KEYPAD      no stuck keys (matrix all-open at rest)
  0x0200  LED         each LED actuated (fixture verifies optically)
  0x0400  BUZZER      chirp actuated (fixture verifies acoustically)
  0x0800  BLE         module reset pulsed (fixture verifies advertising)
```

A unit passes when `pass_mask == executed_mask`. PROVISION is expected to
fail on the first pass (the board is not provisioned yet); the station
provisions, then a re-read (FCT_READ_PROV) confirms it.

### Factory command protocol

Same wire framing as the BLE link — `AA 55 LEN CMD PAYLOAD CRC8`, Sensirion
CRC-8 — so the fixture can reuse one codec. Spoken over the UART test pads
while TEST_MODE is asserted.

```
  CMD   name           payload (station -> unit)   response
  ----  -------------  --------------------------  --------------------------
  0xF0  PING           (none)                      0xE0 INFO
  0xF1  RUN_BIST       (none)                      0xE1 BIST report (20 B)
  0xF2  READ_PROV      (none)                      0xE2 provisioning blob
  0xF3  WRITE_PROV     [papr_provision_t]          0xEF STATUS [0 ok]
  0xF4  SET_OUTPUT     [u8 target][u16 value]      0xEF STATUS
  0xF5  READ_SENSE     (none)                      0xE3 raw sensor snapshot
  0xF6  REBOOT         (none)                      0xEF STATUS, then reset

  INFO  (0xE0)  [maj,min,pat, provisioned?, serial[16]]
  BIST  (0xE1)  [u16 executed][u16 pass][u16 rpm][u16 mA][u16 flow]
                [i16 dP][u32 baro][u16 battery_mv][i16 temp_c10]
  SENSE (0xE3)  [u16 flow][u16 batt_mv][u16 batt_mA][u16 rpm][i16 temp_c10]
  SET_OUTPUT target: 0/1/2 = LED, 3 = buzzer(value=Hz), 4 = blower VREF code
  STATUS code: 0 ok, 1 bad payload, 2 flash error, 3 CRC error, 0xFF unknown
```

### Provisioning record (`papr_provision_t`, written by 0xF3)

```
  off  size  field             notes
  ---  ----  ----------------  ---------------------------------------------
   0    4    magic             firmware sets to 'PRPV' (0x50525056)
   4    2    struct_ver        firmware sets to 1
   6    2    hw_rev            BCD board rev, e.g. 0x0102 = rev 1.2
   8   16    serial            ASCII, NUL-padded (match the laser mark)
  24    4    mfg_date          YYYYMMDD decimal
  28    6    ble_mac           MAC provisioned into the GD32VW553 module
  34    2    reserved0
  36    2    flow_offset_lpm   added to raw flow (per-unit zero)
  38    2    flow_gain_q8      Q8.8 gain, 256 = 1.0 (per-unit span)
  40    2    ibat_offset_ma    shunt-amp zero offset
  42    2    reserved1
  44    4    crc32             firmware (re)computes on write
```

The application calls `papr_provision_apply_flow()` /
`papr_provision_apply_ibat()` so identical firmware yields correct readings
on units that differ within component tolerance. A blank / invalid record
falls back to identity calibration, so an unprovisioned board still runs.

## Mobile App Reference

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
                         +--------+---------+
                                  | "Check for updates"
                                  v
                         +------------------+
                         |   9. UPDATE      |
                         |   (OTA)          |
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
  |  FW version      0.9.0               |  GET_VERSION ACK payload
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
  |  Firmware       v0.9.0               |   <- GET_VERSION
  |  Telemetry rate 500 ms (read-only)   |
  |                                      |
  |  [   RE-PAIR BLE MODULE   ]          |   (BLE PAIR key is on the unit;
  |                                      |    app side just disconnects and
  |                                      |    re-scans)
  |  [   FORGET DEVICE        ]          |
  |  [   CLEAR FAULT          ]          |   -> RESET_FAULT (0x05)
  |  [   CHECK FOR UPDATES   >]          |   -> Update screen (9)
  +--------------------------------------+
```

### 9. Update (OTA)

The app compares the device's running version (GET_VERSION / OTA_STATUS)
against the latest image it can fetch from the vendor's update server. The
firmware never decides "newer exists" — it only reports what it runs.

```
  +--------------------------------------+      +--------------------------------------+
  |  < Settings          Firmware        |      |  < Settings          Firmware        |
  +--------------------------------------+      +--------------------------------------+
  |                                      |      |                                      |
  |   Installed     v0.9.0               |      |   Updating...   do NOT power off     |
  |   Latest        v0.9.0   (available) |      |                                      |
  |                                      |      |   [##########------]  58 %           |  <- OTA_STATUS.pct
  |   * Update can only run while the    |      |   1.2 MB / 2.0 MB                     |
  |     unit is OFF (standby).           |      |                                      |
  |                                      |      |   Stay close to the unit.            |
  |   [   DOWNLOAD & INSTALL   ]         |      |   [   CANCEL   ]                      |  -> OTA_ABORT (0x0D)
  +--------------------------------------+      +--------------------------------------+
        (a) idle / up-to-date or available             (b) transfer in progress

  +--------------------------------------+      +--------------------------------------+
  |  < Settings          Firmware        |      |  Firmware update                     |
  +--------------------------------------+      +--------------------------------------+
  |   Verified OK.  Ready to install.    |      |   [!] Update failed                  |
  |                                      |      |       reason: bad signature (7)      |  <- OTA_STATUS.error
  |   The unit will restart to finish.   |      |                                      |
  |                                      |      |   The current firmware is unchanged. |
  |   [   RESTART & APPLY   ]            |      |   [   TRY AGAIN   ]                   |
  +--------------------------------------+      +--------------------------------------+
        (c) staged & verified (state READY)            (d) error (state ERROR)

  Preconditions enforced by firmware (surface them in the UI):
   - Device must be in STANDBY: if not, OTA_BEGIN -> OTA_STATUS error 1 (BUSY).
     Prompt "Turn the unit off to update."
   - Image must not be a downgrade: error 7 (VERSION).
   - Image must fit the slot (<= ~238 KB usable): error 2 (SIZE).
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
  0x08  SET_AUTO_MODE   [u8 0|1]                ACK 0x81 [0x08]   (auth)
  0x09  OTA_BEGIN       manifest (108 B)        OTA_STATUS 0x84   (auth)
                                                  (state=RECEIVING or error)
  0x0A  OTA_DATA        [u32 off][bytes..124]   ACK 0x81 [0x0A] (per chunk)
                                                  or OTA_STATUS on error  (auth)
  0x0B  OTA_END         (none)                  OTA_STATUS 0x84 (READY|error)
  0x0C  OTA_APPLY       (none)                  ACK on wire, then reboot
                                                  (OTA_STATUS if not READY)
  0x0D  OTA_ABORT       (none)                  OTA_STATUS 0x84 (IDLE)
  0x0E  OTA_STATUS      (none)                  OTA_STATUS 0x84
  0x10  AUTH_BEGIN      (none)                  AUTH_CHALLENGE 0x85
  0x11  AUTH_RESPONSE   [u8 tag[16]]            ACK 0x81 / NACK(4) + SEC_STATUS
  0x12  SEC_STATUS      (none)                  SEC_STATUS 0x86

  (auth) = requires an OPEN session; rejected with NACK reason 5 otherwise.

  Unsolicited from firmware:
  0x80  TELEMETRY       [29 bytes]              every 500 ms + on change
  0x83  EVENT           [u8 type, ...]          on transition
          type 0x01 STATE_CHANGE  [u8 state]
          type 0x02 ALARM_CHANGE  [u32 LE mask]
          type 0x03 LEVEL_CHANGE  [u8 level]
          type 0x04 FAULT         [..]
  0x84  OTA_STATUS      [u8 state, u8 error, u8 pct, u8 maj, u8 min, u8 pat]
          state  0 IDLE  1 RECEIVING  2 READY  3 ERROR
          error  0 NONE 1 BUSY 2 SIZE 3 SEQUENCE 4 ALIGN 5 FLASH
                 6 HASH 7 AUTH 8 ROLLBACK 9 STATE
  0x85  AUTH_CHALLENGE  [u8 nonce[16], u32 LE counter]
  0x86  SEC_STATUS      [u8 auth_state, u8 flags, u16 fail, u16 reject]
          auth_state 0 IDLE 1 CHALLENGED 2 OPEN 3 LOCKED
          flags  0x01 crypto-ok 0x02 auth-req 0x04 signed-OTA 0x08 dbg-locked

  Errors:
  0x82  NACK            [u8 cmd, u8 reason]
          reason 1 = bad/!len payload  2 = unknown cmd  3 = CRC error
                 4 = auth failed        5 = auth required
```

### Use case A — connect, authenticate, and start

```
  app                         firmware (via module)
   |  scan, connect, subscribe TX    |
   |-------------------------------->|
   |  WRITE GET_VERSION (0x06)       |   (read-only, no auth needed)
   |-------------------------------->|
   |        ACK [0,9,0]              |
   |<--------------------------------|
   |  -- authenticate before control --
   |  WRITE AUTH_BEGIN (0x10)        |
   |-------------------------------->|
   |        AUTH_CHALLENGE [nonce,ctr]|
   |<--------------------------------|
   |  tag = HMAC(session_key,        |
   |        nonce||ctr)[0:16]         |
   |  WRITE AUTH_RESPONSE [tag] (0x11)|
   |-------------------------------->|
   |        ACK [0x11]  (session OPEN)|
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

  Without the AUTH exchange, POWER_ON would return NACK reason 5
  (auth required). Telemetry/version reads work before authentication.
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

### Use case E — OTA firmware update

```
  app                                   firmware
   |  Settings: Installed v0.9.0             |
   |  server: latest=v1.0.0, fetch signed    |
   |   image + manifest (size, sha256, sig)  |
   |  authenticate (AUTH_BEGIN/RESPONSE)     |   control requires a session
   |  ensure device is OFF (STANDBY)         |
   |                                         |
   |  WRITE OTA_BEGIN [manifest 108 B]       |
   |---------------------------------------->|  reject if running, downgrade,
   |        OTA_STATUS state=RECEIVING err=0 |  too big -> err 1/8/2; erase slot
   |<----------------------------------------|
   |                                         |
   |  loop over image in <=124-byte chunks:  |
   |   WRITE OTA_DATA [off, bytes]           |
   |---------------------------------------->|  program flash @ off; hash bytes
   |        ACK [0x0A]                        |
   |<----------------------------------------|
   |   ... until all bytes sent ...           |  (progress = off/size)
   |                                         |
   |  WRITE OTA_END                          |
   |---------------------------------------->|  finalize SHA-256; verify hash +
   |        OTA_STATUS state=READY err=0      |  signature + anti-rollback
   |<----------------------------------------|  (err 6 HASH / 7 AUTH / 8 ROLLBACK)
   |                                         |
   |  user taps RESTART & APPLY              |
   |  WRITE OTA_APPLY                        |
   |---------------------------------------->|  bump sec-version, write signed
   |        ACK [0x0C]   (flushed first)     |  manifest trailer (slot PENDING)
   |<----------------------------------------|
   |                          < MCU resets; bootloader verifies + runs new image;
   |                            app confirms healthy boot -> slot VALID >
   |  BLE drops; app shows "restarting"      |
   |  reconnect, authenticate, GET_VERSION   |
   |---------------------------------------->|
   |        ACK [1,0,0]   (now up to date)   |
   |<----------------------------------------|

  Chunk pacing: send the next OTA_DATA only after the previous ACK (simple
  stop-and-wait) — robust and easily fast enough over BLE. On any OTA_STATUS
  with err != 0, stop and surface it; the running firmware is untouched, so
  the user can simply retry. CANCEL at any time -> OTA_ABORT.
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
  - OTA: precompute the CRC-32 (zlib/Ethernet variant) over the exact image
    bytes; send word-aligned, sequential chunks; keep the phone near the unit
    and the screen awake during transfer. Expect the BLE link to drop at
    OTA_APPLY (the MCU resets) and auto-reconnect to confirm the new version.
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
