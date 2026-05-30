# PAPR Firmware — Architecture

ASCII architecture reference for the PAPR (Powered Air-Purifying Respirator)
firmware, from the system hardware down to each individual software component.
Companion to `README.md` (build, pin maps, BLE/mobile protocol). Nothing here
is aspirational — every box and arrow maps to code in `src/`, `include/`, and
`hal/`.

---

## 1. System hardware

```
                          +===========================================+
                          |        GD32E517RE  (Cortex-M33, 180 MHz)   |
                          |        512 KB flash / 128 KB SRAM, LQFP64  |
                          +===========================================+
        analog/I2C/PWM/GPIO |   |   |   |   |   |   |   |   |  UART
        +-------------------+   |   |   |   |   |   |   |   |   +----------------+
        |                       |   |   |   |   |   |   |   |                    |
        v                       v   |   |   |   |   |   |   v                    v
  +-----------+  PWM/DAC  +----------+ |  |   |   |   |  +--------+        +-------------+
  | L6235     |<----------| VREF/EN/ | |  |   |   |   |  | 3x3    |        | GD32VW553   |
  | 3-phase   |  TACHO -->| FWD/BRAKE| |  |   |   |   |  | keypad |        | -UNIFI-EMH7 |
  | BLDC drv  |  DIAG  -->| (timer)  | |  |   |   |   |  +--------+        | BLE module  |
  +-----+-----+           +----------+ |  |   |   |   |                    +------+------+
        |  3-phase                     |  |   |   |   |                           | GATT
        v                              |  |   |   |   |                           v
   +---------+                  ADC ch:|  |   |   |   |GPIO                  +-----------+
   | blower  |   air            VBAT --+  |   |   |   +--- LEDs (OK/WARN/FLT)| mobile app|
   | impeller|=========>        IBAT -----+  |   |   +--- buzzer (TIMER PWM) +-----------+
   +---------+   to hood        FLOW -------+ |   |
                               (NTC) --------+ |   |
                                  I2C0 bus -----+---+----------------+
                                                |                    |
                                          +-----+-----+        +-----+------+
                                          | SDP810    |        | GDY1124    |
                                          | diff. dP  |        | absolute   |
                                          | 0x25      |        | press 0x76 |
                                          +-----------+        +------------+

   +-----------+      +------------------+
   | Li-ion    |----->| buck / rails     |---> 3V3 logic, motor bus
   | 4S pack   |      | + shunt + divider|
   +-----------+      +------------------+
```

Power flows pack -> driver -> blower -> air to the hood. Everything else is
sense (ADC, I2C) and interaction (keypad, LEDs, buzzer, BLE).

---

## 2. Firmware layers

The firmware is strictly layered. Upper layers depend only on the layer
directly beneath; **only the HAL touches MCU registers**.

```
  +-------------------------------------------------------------+
  |  ENTRY            main.c   factory_requested() ? test : app |
  +----------------+--------------------------+-----------------+
                   | app                      | EOL fixture
  +----------------v-------------+   +---------v-----------------+
  |  SUPERVISOR  papr_controller |   | papr_factory (alt. mode)  |
  |  state machine, scheduling,  |   | BIST + provisioning +     |
  |  telemetry, remote latching  |   | actuation over UART pads  |
  +--+----+----+----+----+--+--+-+   +-----+----------+----------+
     |    |    |    |    |  |  |            | (uses sensors/      |
  +--v----v----v----v----v--v--v------------v  drivers + HAL)     |
  |APP / DOMAIN MODULES (portable, no register access)           |
  | blower battery sensors alarms energy keypad ble ota provision|
  +--+------+----+------+----------------------+-+----+----+------+
     |           |      |                       | |    |    | (flash:
  +--v--+   (adc)|   +--v-----+  +-----------+   | |    |    |  ota +
  |CHIP |        |   | SDP810 |  | GDY1124   |   | |    |    |  prov)
  |DRV  |        |   | driver |  | driver    |   | |    |    |
  | l6235        |   +---+----+  +----+------+   | |    |    |
  +--+--+        |       |            | (uart,gpio)|    |    |
  +--v-----------v-------v------------v-----------v-v----v----v----+
  |  HAL contract     papr_hal.h  (pure function declarations)    |
  +------------------------------+-------------------------------+
                                 |
        +-----------------------+----------+----------+----------+
        |                       |          |          |          |
  +-----v---------------+ +-----v--------+ +---------v----------+ |
  | hal/papr_hal_stub.c | | hal/gd32e517re/ | hal/gd32e503ce/   | |
  | host simulation     | | LQFP64 / 512 KB | LQFP48 / 256 KB   | |
  | (PAPR_TARGET=HOST)  | | (=GD32E517RE)   | (=GD32E503CE)     | |
  +---------------------+ +-----------------+-------------------+ |
                                  |                   |           |
                                  v                   v           |
                              GD32E51x SDK        GD32E50x SDK    |
                              (gd32e51x.h)        (gd32e50x.h)    |
```

The bottom HAL row is open-ended on purpose: new MCU choices add a sibling
directory under `hal/` and a `PAPR_TARGET=` value without touching anything
above the HAL contract.

`papr_config.h` and `papr_types.h` are leaf headers included everywhere
(compile-time tuning + shared enums/structs); they sit beside every layer
rather than in the stack.

---

## 3. Module composition & dependency graph

Solid `==>` = "owns an instance of" (struct embedding); thin `-->` = "calls
into". The controller owns one of each domain module; two modules in turn own
chip drivers.

```
                          papr_controller_t
   +--------+--------+------+-----+-----+------+-----+------+
   ||       ||       ||     ||    ||    ||     ||     ||
   v|       v|       v|     v|    v|    v|     v|     v|
  blower  battery sensors alarms energy keypad ble   ota
   ||               ||  ||                       |    |
   v|               v|  v|                        |    | calls
  l6235          sdp810 gdy1124                   |    | remote_* /
   |               |    |                         |    | snapshot /
   |               |    |                         |    | ota_*
   +---------------+----+----------+--------+------+----+
                   |               |        |      |    | (flash:
                   v               v        v      v    v  erase/write/
              papr_hal.h  (drivers + alarms/keypad/ble/ota/    read/
                           battery/sensors/controller call HAL) commit/reboot)
```

HAL call-count per module (a quick read of who is hardware-heavy):

```
  l6235  17 | ble 10 | alarms 6 | controller 5 | keypad 4 | sdp810 4
  gdy1124 3 | battery 2 | sensors 2 | ota 3 | blower 0 | energy 0 | main 0
```

`ota` calls the HAL flash/reset group; `ble` carries the OTA command frames
and forwards them to the controller's `ota_*` wrappers (which gate on idle).

`blower` and `energy` make **zero** direct HAL calls — `blower` delegates all
hardware to `l6235`; `energy` is a pure algorithm over values handed to it.

---

## 4. Boot & main loop

In a dual-bank/secure build the bootloader (sec. 10) runs first on reset,
verifies a slot's signed manifest, and jumps to that slot's vector table; the
flow below is then the *application's* entry. In the single-image dev build
the app is the reset image directly.

```
  [bootloader verifies + jumps]  ->  app vector table
    |
    v
  startup_gd32e51x.s  (vendor)  -> SystemInit() -> main()
    |
    v
  papr_hal_factory_requested()?  (reads TEST_MODE pad, pre-init safe)
    |                        \
    | no (field unit)         \ yes (EOL fixture)
    v                          v
  papr_controller_init()     papr_hal_init() ; papr_factory_main()
    |  papr_hal_init()           |  BIST + provisioning + actuation over
    |  blower/battery/sensors    |  the UART test pads; FCT_REBOOT resets
    |  alarms/ble/keypad/energy  |  into the application. (sec. 11)
    |  ota init ; state = INIT
    v
  +-------------------------------------------------------+
  |  for (;;) papr_controller_step()                      |
  |                                                       |
  |   1. watchdog kick        if >= 100 ms                |
  |   2. sensors + battery    if >= 20 ms                 |
  |   3. dispatch_keys        keypad scan (self-paced 8ms)|
  |   4. ble_poll             drain RX, telemetry @ 500ms |
  |      (also handles OTA command frames)                |
  |   5. apply_remote_requests latched BLE commands       |
  |   6. energy_update        every tick                  |
  |   7. auto-level apply      if auto mode on            |
  |   8. STATE MACHINE step   (see sec. 5)                |
  |   9. alarms_render        LEDs + buzzer               |
  +-------------------------------------------------------+
```

The loop is **non-blocking and self-pacing**: `papr_controller_step()` runs as
fast as the CPU allows and each sub-task gates itself on elapsed milliseconds
from `papr_hal_now_ms()`. No RTOS required.

---

## 5. Supervisor state machine

```
                +--------+
                |  INIT  |
                +---+----+
                    |
                    v
              +-----------+   sensors/battery bad > 3 s
              | SELF_TEST |--------------------------+
              +-----+-----+                          |
                    | pass                           |
                    v                                |
              +-----------+   POWER long-press        |
        +---->| STANDBY   |----------+               |
        |     +-----------+          | start blower  |
        |          ^                 v               |
        |          |           +-----------+         |
   shutdown done   |           | RUNNING   |         |
        |          |           +-----+-----+         |
        |          |   POWER long-pr |  ^  critical  |
        |     +----+-----+   or BLE  |  |  alarm     |
        +-----| SHUTDOWN |<----------+  |            |
              +----------+              v            |
                    ^             +-----------+      |
                    | cutoff /    |  ALARM    |      |
                    | motor fault |  (air ON, |      |
                    +-------------+  buzzer)  |      |
                                  +-----+-----+      |
                                        | alarm clears
                                        +---> RUNNING |
                                                      |
                                              +-------v----+
                                              |  FAULT     |
                                              | (blower    |
                                              |  stopped)  |
                                              +-----+------+
                                                    | RESET key / BLE
                                                    +---> INIT
```

Key safety rule encoded here: in **ALARM** the blower keeps running (the worker
must not lose air); only battery cutoff or a motor fault forces SHUTDOWN. A hard
**FAULT** stops the blower and waits for an explicit RESET.

---

## 6. Control & data flow (one running tick)

```
  SENSE                     DECIDE                      ACT
  -----                     ------                      ---
  ADC flow ----+
  ADC temp ----+--> sensors --+
  SDP810 dP ---+              |
  GDY1124 -----+              |
                              v
  ADC vbat/ibat --> battery --+--> controller ----> blower.update(PID)
                              |        |                  |
  keypad -------------------->+        |                  v
  BLE rx commands ----------->+        |             l6235.set_current
                              |        |                  |
                              |        |                  v
                              |        |             DAC VREF -> motor
                              |        |
                       energy.update   +--> alarms.evaluate --> LEDs+buzzer
                       (breath, runtime)|
                              |         |
                              v         v
                          auto-level   telemetry snapshot
                          suggestion        |
                              |             v
                              +---------> ble.send_telemetry --> mobile app
```

`flow_lpm` is the PID feedback; `pressure_pa` (SDP810) feeds both telemetry and
the energy module's breathing detector; `absolute_pressure_pa` (GDY1124) is
telemetry-only.

---

## 7. Component reference

Each entry: role, primary type, who owns it, and its HAL surface.

```
+---------------------------------------------------------------------------+
| papr_controller            owner: main.c                                  |
|   Supervisory state machine + scheduler. Owns every domain module.        |
|   Latches BLE remote requests, applies auto-level, builds telemetry.      |
|   type: papr_controller_t      HAL: now_ms, wdt_kick, init                |
+---------------------------------------------------------------------------+
| papr_blower                owner: controller                              |
|   Closed-loop airflow PID. Setpoint = L/min, output = motor current (mA). |
|   Anti-windup, saturation, demand-permille for clog detection.            |
|   type: papr_blower_t  ==> papr_l6235_t      HAL: (none, via l6235)       |
+---------------------------------------------------------------------------+
| papr_l6235                 owner: papr_blower                             |
|   ST L6235 3-phase BLDC driver. mA -> VREF DAC (I=VREF/Rsense), EN/FWD/   |
|   BRAKE, DIAG fault latch, TACHO rpm.                                     |
|   type: papr_l6235_t      HAL: l6235_set_vref/enable/forward/brake,       |
|                                l6235_diag_active, read_tacho_rpm          |
+---------------------------------------------------------------------------+
| papr_battery               owner: controller                             |
|   Pack voltage/current filtering, SoC estimate, OK/LOW/CRIT/CUTOFF level. |
|   type: papr_battery_t     HAL: read_battery_mv, read_battery_ma          |
+---------------------------------------------------------------------------+
| papr_sensors               owner: controller                             |
|   Aggregates flow + temperature (ADC) and owns the two pressure drivers.  |
|   Validity flags per channel feed the SENSOR_FAULT alarm.                 |
|   type: papr_sensors_t  ==> papr_sdp810_t, papr_gdy1124_t                 |
|                            HAL: read_flow_lpm, read_temperature_c10       |
+---------------------------------------------------------------------------+
| papr_sdp810                owner: papr_sensors                           |
|   Sensirion SDP810-500Pa diff. pressure. Continuous-mode start, 9-byte    |
|   frames, triple Sensirion CRC-8, scale factor 60 LSB/Pa.                 |
|   type: papr_sdp810_t      HAL: i2c_write, i2c_read                       |
|   note: exports papr_sdp810_crc8() reused by ble + stub                   |
+---------------------------------------------------------------------------+
| papr_gdy1124               owner: papr_sensors                           |
|   GigaDevice GDY1124 absolute pressure. Chip-ID, 24-byte calibration,     |
|   64-bit fixed-point compensation -> Pa + 0.1 C.                          |
|   type: papr_gdy1124_t     HAL: i2c_write, i2c_read                       |
+---------------------------------------------------------------------------+
| papr_alarms                owner: controller                             |
|   Debounced latching of 7 alarm bits; mute timer; LED + buzzer rendering  |
|   (critical = steady 4 kHz, warning = 2 kHz 1 Hz cadence).                |
|   type: papr_alarms_t      HAL: led_set, buzzer_set                       |
+---------------------------------------------------------------------------+
| papr_energy                owner: controller                             |
|   Adaptive comfort: breathing rate/amplitude from dP, auto-level          |
|   suggestion (30 s hold-off, never below LOW), coulomb-counter runtime,   |
|   per-session EWMA. Pure algorithm.                                       |
|   type: papr_energy_t      HAL: (none)                                    |
+---------------------------------------------------------------------------+
| papr_keypad                owner: controller                             |
|   3x3 switch matrix scan, debounce, PRESS/RELEASE/LONG_PRESS, event FIFO. |
|   type: papr_keypad_t      HAL: keypad_drive_row, keypad_read_col         |
+---------------------------------------------------------------------------+
| papr_ble                   owner: controller                             |
|   GD32VW553 link. Framed binary protocol (AA 55 LEN CMD PAYLOAD CRC8),    |
|   RX state machine -> controller_remote_*/ota_* hooks, periodic telemetry |
|   + state/level/alarm EVENTs, OTA_STATUS notifications.                   |
|   type: papr_ble_t         HAL: uart_write, uart_read_byte,               |
|                                 ble_set_reset, ble_host_wake              |
+---------------------------------------------------------------------------+
| papr_ota                   owner: controller                             |
|   OTA receiver. Stages an image in the inactive flash slot, verifies a    |
|   whole-image CRC-32, commits boot metadata + resets on apply. Idle-      |
|   gated; anti-downgrade; never erases the running image (recoverable).    |
|   type: papr_ota_t         HAL: ota_slot_size, ota_erase, ota_write,      |
|                                 ota_read, ota_commit, ota_reboot, wdt_kick|
+---------------------------------------------------------------------------+
| papr_provision             owner: app sensor path + papr_factory         |
|   Per-unit identity + calibration in a dedicated flash page (serial, HW   |
|   rev, mfg date, BLE MAC, flow/current cal). CRC-guarded; survives OTA.    |
|   type: papr_provision_t   HAL: prov_read, prov_write, unique_id          |
+---------------------------------------------------------------------------+
| papr_factory               owner: main.c (alternate boot mode)           |
|   End-of-line BIST + factory command loop over the UART test pads (BLE    |
|   framing). Drives every subsystem via the HAL, returns pass/fail + cal   |
|   measurements, reads/writes provisioning. Entered only when TEST_MODE    |
|   is asserted at boot.                                                     |
|   types: papr_bist_report_t  HAL: every actuation/read group + uart +     |
|                                   prov + ota_reboot                       |
+---------------------------------------------------------------------------+
| papr_sha256                owner: secure / ota / boot                     |
|   Self-contained SHA-256 + HMAC-SHA256, known-answer self-test. No HAL    |
|   deps, so the bootloader links it directly.                              |
+---------------------------------------------------------------------------+
| papr_secure                owner: controller                             |
|   Cybersecurity: HMAC challenge/response session auth gating control,     |
|   signed + anti-rollback image verification, audit counters, lockout.     |
|   type: papr_secure_t      HAL: rng, sec_key_read, sec_verify,            |
|                                 sec_version_get/set, debug_locked          |
+---------------------------------------------------------------------------+
| papr_boot (bootloader)     standalone image, not linked into the app      |
|   Verified boot: re-checks each slot's manifest (image hash + vendor       |
|   signature) every reset; one-try rollback (PENDING->TRYING->VALID);      |
|   recovery loop if nothing verifies. Reuses papr_sha256 only.             |
+---------------------------------------------------------------------------+
```

Leaf headers (no logic):

```
  papr_types.h    status enum, level/state enums, alarm bitmask,
                  papr_telemetry_t (the cross-module data record)
  papr_config.h   every compile-time knob (flow setpoints, PID gains,
                  battery thresholds, I2C addresses, OTA slot, timings)
  papr_version.h  firmware version (single source of truth) + compare
                  helper, used by ble GET_VERSION and ota anti-downgrade
  papr_hal.h      the hardware contract — the only seam between portable
                  code and silicon
```

---

## 8. HAL contract & porting

The HAL is the portability boundary. To bring up new silicon, implement every
declaration in `papr_hal.h`; the entire stack above compiles unchanged.

```
  papr_hal.h groups:
   time        now_ms
   motor       l6235_set_vref / set_enable / set_forward / set_brake
               l6235_diag_active / read_tacho_rpm
   analog      read_flow_lpm / read_battery_mv / read_battery_ma /
               read_temperature_c10
   i2c         i2c_write / i2c_read            (shared: SDP810 + GDY1124)
   uart/ble    uart_write / uart_read_byte / ble_set_reset / ble_host_wake
   keypad      keypad_drive_row / keypad_read_col
   ui          led_set / buzzer_set / (legacy) button_* 
   safety      wdt_kick
   ota/flash   ota_slot_size / ota_erase / ota_write / ota_read /
               ota_commit / ota_reboot / ota_confirm
   test/prov   factory_requested / unique_id / prov_read / prov_write
   security    rng / sec_key_read / sec_verify / sec_version_get/set /
               secure_lock_debug / debug_locked
   lifecycle   init

  Two implementations ship:
   hal/papr_hal_stub.c        host build; simulates motor/sensors/I2C
                              devices so the firmware runs on a PC.
   hal/gd32e517re/            real port: DAC, ADC scan, I2C0 master,
     papr_hal_gd32e517re.c    USART0 + RX ring ISR, TIMER capture/PWM,
     papr_pinmap.h            FWDGT watchdog, SysTick 1 ms.
     gd32e517re.ld
```

---

## 9. Timing model

```
  task               cadence          gated by
  -----------------  ---------------  -------------------------------
  controller_step    free-run         (tight loop in main)
  watchdog kick      >= 100 ms        PAPR_WDT_KICK_MS
  sensors+battery    >= 20 ms         PAPR_SENSOR_PERIOD_MS
  blower PID         >= 10 ms         PAPR_CONTROL_PERIOD_MS
  keypad scan        >= 8 ms          PAPR_KEYPAD_SCAN_PERIOD_MS
  energy update      every step       (internal dt for coulomb count)
  BLE telemetry      >= 500 ms        PAPR_BLE_TELEM_PERIOD_MS
  SysTick tick       1 ms             hardware (GD32 port)
```

All pacing derives from one monotonic source, `papr_hal_now_ms()`; wrap-safe
unsigned subtraction is used everywhere (`(now - last) >= period`).

OTA work runs inside the normal loop: BLE OTA frames are handled in step 4,
flash erase/verify kick the watchdog directly, and an apply resets the MCU.

---

## 10. OTA flash map + secure boot (rev 9)

```
  GD32E517RE flash, 512 KB
  0x08000000  +-------------------+  bootloader (28 KB): SHA-256 self-test,
              |  bootloader       |   verifies each slot's signed manifest,
  0x08007000  +-------------------+   one-try rollback, jumps to a slot.
              |  boot-state page  |  mutable per-slot lifecycle + sec-version
  0x08007800  +-------------------+   floor (bootloader/HAL rewrite it).
              |  provisioning page|  serial / cal / KEYS, written by the EOL
  0x08008000  +-------------------+   station; read-protected after test.
              |  slot A (app)     |  238 KB app + 2 KB SIGNED manifest trailer
              |  + manifest @end  |   (magic|size|sec_ver|fw|sha256|signature).
  0x08044000  +-------------------+
              |  slot B (app)     |  the running image lives in one slot;
              |  + manifest @end  |  papr_ota stages the new image into the
  0x08080000  +-------------------+  OTHER slot (chosen from SCB->VTOR).

  secure update + boot chain
  --------------------------
   OTA receive          OTA apply               next reset (bootloader)
    ble OTA_DATA         ble OTA_APPLY            verify slot manifest:
      |                    |                       sha(image)==manifest.sha?
      v                    v                       sig(manifest) ok?
   ota_write +          ota_finish: hash+sig+      sec_version >= floor?
   sha256_update        rollback verify            |
      |                    |                        +-- PENDING -> TRYING,
      v                    v                        |   boot once
   hal_ota_write        hal_ota_commit(manifest)    +-- app self-test ok ->
   (staging slot)       -> slot PENDING                 ota_confirm -> VALID
                        hal_ota_reboot              +-- no confirm -> rollback

  Integrity   SHA-256 over the image, verified at OTA_END and again at boot.
  Authentic   vendor signature over the manifest (HMAC ref / ECDSA prod).
  Anti-roll   monotonic sec_version floor; older images refused.
  No-brick    running slot never erased; bad/unconfirmed image rolled back.
```

---

## 11. Build / target matrix

```
  PAPR_TARGET = HOST            (default)
    sources: papr_core + src/main.c + hal/papr_hal_stub.c
    output : native ELF, runs the full firmware against simulated hardware

  PAPR_TARGET = GD32E517RE      LQFP64, 512 KB, 128 KB SRAM
    requires: arm-none-eabi-gcc toolchain file + GD32E51X_SDK_DIR
    sources: papr_core + src/main.c + hal/gd32e517re/* + GD32E51x startup
    flags  : -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard
    link   : hal/gd32e517re/gd32e517re.ld           (single-image dev), or
             linker/{app_slot_a,b}.ld               (dual-bank)
    app    : 240 KB slots; 2 KB flash page

  PAPR_TARGET = GD32E503CE      LQFP48, 256 KB, 64 KB SRAM  (alternative MCU)
    requires: arm-none-eabi-gcc toolchain file + GD32E50X_SDK_DIR
    sources: papr_core + src/main.c + hal/gd32e503ce/* + GD32E50x startup
    flags  : -mcpu=cortex-m33 -mfpu=fpv5-sp-d16 -mfloat-abi=hard
    link   : hal/gd32e503ce/gd32e503ce.ld           (single-image dev), or
             hal/gd32e503ce/{app_slot_a,b}.ld       (dual-bank)
    app    : 112 KB slots; 1 KB flash page
             (pinmap overrides PAPR_SLOT_* defaults in boot_shared.h)

  papr_core  =  blower battery sensors alarms l6235 sdp810 gdy1124
                ble keypad energy ota provision factory sha256 secure
                controller          (same library for both MCU targets)

  bootloader  =  boot/papr_boot.c + papr_sha256.c  (separate image,
                 PAPR_BUILD_BOOTLOADER, per-MCU bootloader.ld)
  app slots   =  PAPR_APP_SLOT=A|B -> per-MCU app_slot_{a,b}.ld
                 (unset -> single-image dev build)
```

---

## 12. Directory map

```
  papr/
  |- README.md            build, pin maps, BLE + mobile-app + OTA protocol
  |- ARCHITECTURE.md      this document
  |- CMakeLists.txt       HOST vs GD32E517RE target selection
  |- include/
  |    papr_types.h  papr_config.h  papr_hal.h        (leaf contracts)
  |    papr_version.h                                 (fw version)
  |    papr_controller.h                              (supervisor)
  |    papr_blower.h papr_l6235.h                     (actuation)
  |    papr_battery.h papr_sensors.h
  |    papr_sdp810.h papr_gdy1124.h                   (sensor drivers)
  |    papr_alarms.h papr_energy.h papr_keypad.h papr_ble.h papr_ota.h
  |    papr_provision.h papr_factory.h                (DFM / DFT)
  |    papr_sha256.h papr_secure.h                    (cybersecurity)
  |- src/
  |    main.c            <entry: app vs factory mode>
  |    papr_controller.c <supervisor>
  |    papr_blower.c papr_l6235.c
  |    papr_battery.c papr_sensors.c
  |    papr_sdp810.c papr_gdy1124.c
  |    papr_alarms.c papr_energy.c papr_keypad.c papr_ble.c papr_ota.c
  |    papr_provision.c papr_factory.c
  |    papr_sha256.c papr_secure.c
  |- boot/                                            (secure boot, rev 9)
  |    boot_shared.h     image manifest + boot-state + flash map
  |    papr_boot.c       standalone bootloader (GD32-guarded)
  |- ci/
  |    gd32_shim/        permissive vendor-SDK shim for CI syntax-check
  |                      (NOT a working SDK — see gd32_periph.h)
  |- cmake/
  |    arm-none-eabi.cmake   CMake toolchain file for the cross-compile
  |                          (used by nightly + release CI lanes)
  |- linker/
  |    papr_sections.ld  shared SECTIONS body
  |    bootloader.ld  app_slot_a.ld  app_slot_b.ld   (GD32E517RE)
  |- hal/
       papr_hal_stub.c                              (HOST)
       gd32e517re/                                  (LQFP64 / 512 KB)
         papr_hal_gd32e517re.c  papr_pinmap.h
         gd32e517re.ld          (single-image dev layout)
       gd32e503ce/                                  (LQFP48 / 256 KB)
         papr_hal_gd32e503ce.c  papr_pinmap.h
         gd32e503ce.ld          (single-image dev layout)
         bootloader.ld  app_slot_a.ld  app_slot_b.ld (dual-bank)
```

---

## 13. Cross-references

```
  build & flashing ............. README.md  "Building"
  GD32E517RE pin map ........... README.md  "GD32E517RE pin map"
  sensor wiring ................ README.md  "SDP810" / "GDY1124"
  BLE wire protocol ............ README.md  "BLE wireless control"
  mobile app screens/use cases . README.md  "Mobile App Reference"
  OTA update flow .............. README.md  "OTA firmware update"
  cybersecurity / auth ......... README.md  "Cybersecurity"
  secure boot + dual-bank ...... README.md  "Secure boot & dual-bank"
  production test / station .... README.md  "Production test (DFM / DFT)"
  CI lanes (host + GD32 ports) . README.md  "Continuous integration"
  safety caveats ............... README.md  "Safety notes"
```

This is a reference design, not certified medical-device firmware — see the
safety notes in `README.md` before any real-world use.
