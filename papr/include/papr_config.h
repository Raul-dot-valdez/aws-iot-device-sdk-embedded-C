#ifndef PAPR_CONFIG_H
#define PAPR_CONFIG_H

/* Compile-time configuration for the PAPR controller.
 * Values target a typical 32-bit ARM Cortex-M MCU running at 64-180 MHz.
 * Override any define from the build system (-D...) to retune for hardware. */

/* Control loop period in milliseconds. The main supervisor runs at this rate. */
#ifndef PAPR_CONTROL_PERIOD_MS
#define PAPR_CONTROL_PERIOD_MS          10U
#endif

/* Sensor sampling period in milliseconds. */
#ifndef PAPR_SENSOR_PERIOD_MS
#define PAPR_SENSOR_PERIOD_MS           20U
#endif

/* Airflow setpoints in litres-per-minute for the three user-selectable levels.
 * EN 12941 minimum design flow for TH3 class is 160 L/min. */
#ifndef PAPR_FLOW_LOW_LPM
#define PAPR_FLOW_LOW_LPM               170U
#endif
#ifndef PAPR_FLOW_MED_LPM
#define PAPR_FLOW_MED_LPM               195U
#endif
#ifndef PAPR_FLOW_HIGH_LPM
#define PAPR_FLOW_HIGH_LPM              220U
#endif

/* Minimum acceptable instantaneous flow before the low-flow alarm fires. */
#ifndef PAPR_FLOW_ALARM_LPM
#define PAPR_FLOW_ALARM_LPM             150U
#endif

/* Time the flow must stay below the threshold before the alarm latches. */
#ifndef PAPR_FLOW_ALARM_DEBOUNCE_MS
#define PAPR_FLOW_ALARM_DEBOUNCE_MS     2000U
#endif

/* Battery thresholds in millivolts for a 4-cell Li-ion pack (14.4 V nominal). */
#ifndef PAPR_BATT_FULL_MV
#define PAPR_BATT_FULL_MV               16800
#endif
#ifndef PAPR_BATT_LOW_MV
#define PAPR_BATT_LOW_MV                13200
#endif
#ifndef PAPR_BATT_CRITICAL_MV
#define PAPR_BATT_CRITICAL_MV           12600
#endif
#ifndef PAPR_BATT_CUTOFF_MV
#define PAPR_BATT_CUTOFF_MV             12000
#endif

/* Filter loading is inferred from the controller demand required to hit the
 * setpoint. If the demand saturates above this fraction (in tenths of percent)
 * for the debounce interval, the filter-clog alarm fires. */
#ifndef PAPR_FILTER_CLOG_DUTY_PERMILLE
#define PAPR_FILTER_CLOG_DUTY_PERMILLE  950U
#endif
#ifndef PAPR_FILTER_CLOG_DEBOUNCE_MS
#define PAPR_FILTER_CLOG_DEBOUNCE_MS    3000U
#endif

/* PID gains for the airflow control loop. Output units are motor current in
 * milliamps (see PAPR_L6235_IMAX_MA below). Q16.16 fixed-point. */
#ifndef PAPR_PID_KP_Q16
#define PAPR_PID_KP_Q16                 (12 << 16)
#endif
#ifndef PAPR_PID_KI_Q16
#define PAPR_PID_KI_Q16                 (3 << 16)
#endif
#ifndef PAPR_PID_KD_Q16
#define PAPR_PID_KD_Q16                 (1 << 16)
#endif

/* ---- L6235 brushless DC driver --------------------------------------------
 * The L6235 is a fully integrated three-phase DMOS driver with internal Hall
 * decoding and constant-tOFF PWM current regulation. The MCU sets the peak
 * phase current via VREF (analog input) and toggles enable / direction / brake
 * GPIOs. Speed feedback comes from the TACHO open-drain output. Faults
 * (overcurrent, thermal shutdown) are signalled on DIAG (open-drain, low). */

/* Sense resistor on the SENSE pin, milliohms. Datasheet recommends 0.3 Ω
 * giving ≈ 2.8 A peak at VREF = 0.84 V (peak limited by package thermals). */
#ifndef PAPR_L6235_RSENSE_MOHM
#define PAPR_L6235_RSENSE_MOHM          300U
#endif

/* Maximum VREF voltage the MCU is allowed to drive into the L6235 VREF pin,
 * in millivolts. Below the L6235 absolute max of 7 V; sized for the analog
 * front-end the MCU can produce (typ. 3.0 V from a DAC or filtered PWM). */
#ifndef PAPR_L6235_VREF_MAX_MV
#define PAPR_L6235_VREF_MAX_MV          2500U
#endif

/* Peak motor phase current that corresponds to PAPR_L6235_VREF_MAX_MV.
 * I_peak[A] = VREF[V] / RSENSE[Ω].  2500 mV / 0.3 Ω ≈ 8.3 A — capped here
 * to 2500 mA to stay within the L6235 continuous rating with adequate
 * heatsinking and to protect the blower windings. */
#ifndef PAPR_L6235_IMAX_MA
#define PAPR_L6235_IMAX_MA              2500U
#endif

/* Minimum current to keep the rotor commutating cleanly during light load. */
#ifndef PAPR_L6235_IMIN_MA
#define PAPR_L6235_IMIN_MA              150U
#endif

/* TACHO output: number of pulses per mechanical revolution. The L6235 emits
 * one TACHO edge per Hall transition, so 6 × pole pairs per revolution. */
#ifndef PAPR_L6235_TACHO_PPR
#define PAPR_L6235_TACHO_PPR            12U   /* 2 pole-pair motor */
#endif

/* DIAG debounce. The pin can twitch during commutation; ignore pulses shorter
 * than this before declaring a motor fault. */
#ifndef PAPR_L6235_DIAG_DEBOUNCE_MS
#define PAPR_L6235_DIAG_DEBOUNCE_MS     5U
#endif

/* Resolution of the VREF DAC channel driving the L6235. 12-bit is plenty for
 * smooth current control at the chip's tens-of-kHz internal PWM. */
#ifndef PAPR_L6235_VREF_DAC_MAX
#define PAPR_L6235_VREF_DAC_MAX         4095U
#endif

/* ---- Sensirion SDP810-500Pa differential pressure sensor ------------------
 * 4-pin tube-connection variant. I²C interface, 7-bit address 0x25 by default,
 * ±500 Pa range, scale factor 60 LSB/Pa, on-chip temperature compensation. */

#ifndef PAPR_SDP810_I2C_ADDR
#define PAPR_SDP810_I2C_ADDR            0x25U
#endif

/* Scale factor for the −500..+500 Pa variant (datasheet table 6). */
#ifndef PAPR_SDP810_SCALE
#define PAPR_SDP810_SCALE               60
#endif

/* Time the chip needs after a "start continuous measurement" command before
 * the first 9-byte read can succeed. The datasheet quotes 8 ms typ; round up. */
#ifndef PAPR_SDP810_STARTUP_DELAY_MS
#define PAPR_SDP810_STARTUP_DELAY_MS    20U
#endif

/* Hard limit on how often we issue the start command. Once continuous mode
 * is running the chip refreshes its internal registers every 0.5 ms, so the
 * sensors module just reads the latest 9-byte snapshot. */
#ifndef PAPR_SDP810_RESTART_PERIOD_MS
#define PAPR_SDP810_RESTART_PERIOD_MS   60000U
#endif

/* Watchdog kick interval. Must be shorter than the hardware watchdog window. */
#ifndef PAPR_WDT_KICK_MS
#define PAPR_WDT_KICK_MS                100U
#endif

#endif /* PAPR_CONFIG_H */
