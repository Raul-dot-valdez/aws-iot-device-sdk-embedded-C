# PAPR Firmware

Reference firmware for a Powered Air-Purifying Respirator (PAPR) running on
32-bit microcontrollers (ARM Cortex-M, RISC-V, etc.). Written in portable C11.

## Layout

```
papr/
├── include/        Public headers (config, types, HAL contract, modules)
├── src/            Portable controller core
├── hal/            Reference HAL stub (replace per target MCU)
└── CMakeLists.txt  Host build for verification
```

## Modules

- `papr_controller` — supervisory state machine (init → self-test → standby →
  running → alarm → shutdown / fault).
- `papr_blower` — closed-loop airflow control with PID, anti-windup, and PWM
  saturation handling.
- `papr_battery` — voltage filtering, state-of-charge estimation, and
  level classification (OK / low / critical / cutoff).
- `papr_sensors` — flow, pressure, temperature, and motor-RPM acquisition.
- `papr_alarms` — debounced alarm latching and audible/visual rendering.
- `papr_hal` — vendor-agnostic hardware abstraction.

## Porting

1. Implement every function declared in `include/papr_hal.h` for your target
   (PWM timer, ADC for battery and flow, GPIO for buttons, I²C for the
   pressure sensor, watchdog, millisecond tick).
2. Tune `include/papr_config.h` to match the blower curve, battery chemistry,
   and certification target (EN 12941 / NIOSH 42 CFR 84).
3. Replace `hal/papr_hal_stub.c` with your implementation.

## Safety notes

This is a reference design, **not** a certified medical device firmware. Any
real-world deployment requires:

- IEC 62304 software lifecycle compliance.
- Independent watchdog with a separate clock domain.
- Redundant flow sensing or motor-current backup for the low-flow alarm.
- Validated battery fuel-gauge IC instead of pure voltage estimation.
- Field testing against the applicable PAPR standard.
