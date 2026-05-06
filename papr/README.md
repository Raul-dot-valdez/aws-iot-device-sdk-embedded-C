# PAPR Firmware

Reference firmware for a Powered Air-Purifying Respirator (PAPR) running on
32-bit microcontrollers (ARM Cortex-M, RISC-V, etc.). Written in portable C11.

**Revision 2** — the blower stage is driven by an
[ST L6235](https://www.st.com/en/motor-drivers/l6235.html) three-phase DMOS
brushless DC driver. The MCU no longer generates bridge PWM itself; it sets
a peak-current reference and lets the L6235 handle commutation and chopping.

## Layout

```
papr/
├── include/        Public headers (config, types, HAL contract, modules)
├── src/            Portable controller core (incl. L6235 driver)
├── hal/            Reference HAL stub (replace per target MCU)
└── CMakeLists.txt  Host build for verification
```

## Modules

- `papr_controller` — supervisory state machine (init → self-test → standby →
  running → alarm → shutdown / fault).
- `papr_blower` — closed-loop airflow control. PID setpoint is litres-per-min;
  PID output is **motor current in mA**, written to the L6235 VREF input.
  Anti-windup, derivative on error, output saturation.
- `papr_l6235` — chip-specific driver. Translates current-mA setpoints into
  VREF DAC codes (`I_peak = VREF / R_sense`), drives EN / FWD / BRAKE,
  monitors DIAG (overcurrent / thermal-shutdown) and TACHO (RPM feedback).
- `papr_battery` — voltage filtering, state-of-charge estimation, and
  level classification (OK / low / critical / cutoff).
- `papr_sensors` — flow, differential pressure, and temperature acquisition.
- `papr_alarms` — debounced alarm latching, audible/visual rendering.
- `papr_hal` — vendor-agnostic hardware abstraction.

## L6235 wiring

| L6235 pin | Direction | MCU peripheral                          |
| --------- | --------- | --------------------------------------- |
| `VREF`    | analog in | DAC channel (or PWM through RC filter)  |
| `EN`      | input     | GPIO output, push-pull                  |
| `FWD/REV` | input     | GPIO output (held high = forward)       |
| `BRAKE`   | input     | GPIO output, push-pull (active LOW)     |
| `DIAG`    | open-drain out | GPIO input + pull-up, EXTI on falling edge |
| `TACHO`   | open-drain out | TIM input-capture, PWM-input mode  |
| `SENSE`   | analog    | through `R_sense` (0.3 Ω typical) to GND |
| `VBOOT/VCP` | —       | charge-pump capacitor per datasheet     |

The current-to-VREF relationship is

```
I_peak [A] = V_REF [V] / R_sense [Ω]
```

so with the defaults in `papr_config.h` (`R_sense = 0.3 Ω`,
`V_REF_max = 2.5 V`) the controller spans 0 – 2.5 A peak phase current.

## Porting

1. Implement every function declared in `include/papr_hal.h` for your target
   MCU: DAC, GPIO, EXTI, timer input-capture, ADC for battery and flow,
   I²C for the pressure sensor, watchdog, millisecond tick.
2. Wire the L6235 as in the table above and tune
   `PAPR_L6235_RSENSE_MOHM`, `PAPR_L6235_VREF_MAX_MV`, and
   `PAPR_L6235_IMAX_MA` in `include/papr_config.h` to your blower.
3. Replace `hal/papr_hal_stub.c` with your implementation.
4. Re-tune the PID gains (`PAPR_PID_K{P,I,D}_Q16`) for the
   blower-impeller-filter combination — the units changed from
   "PWM duty per LPM error" in revision 1 to "motor mA per LPM error" here.

## Safety notes

This is a reference design, **not** a certified medical device firmware. Any
real-world deployment requires:

- IEC 62304 software lifecycle compliance.
- Independent watchdog with a separate clock domain.
- Redundant flow sensing or motor-current backup for the low-flow alarm.
- Validated battery fuel-gauge IC instead of pure voltage estimation.
- Confirmation that the L6235 thermal package (PowerSO20 / SO24) and
  external `R_sense` dissipation match the target ambient and duty cycle.
- Field testing against the applicable PAPR standard (EN 12941, NIOSH 42 CFR 84).
