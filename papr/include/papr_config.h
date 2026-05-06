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

/* Filter loading is inferred from the duty cycle required to hit the setpoint.
 * If the controller saturates above this fraction (in tenths of percent) for
 * the debounce interval, the filter-clog alarm fires. */
#ifndef PAPR_FILTER_CLOG_DUTY_PERMILLE
#define PAPR_FILTER_CLOG_DUTY_PERMILLE  950U
#endif
#ifndef PAPR_FILTER_CLOG_DEBOUNCE_MS
#define PAPR_FILTER_CLOG_DEBOUNCE_MS    3000U
#endif

/* PID gains for the airflow control loop. Q16.16 fixed-point. */
#ifndef PAPR_PID_KP_Q16
#define PAPR_PID_KP_Q16                 (45 << 16)
#endif
#ifndef PAPR_PID_KI_Q16
#define PAPR_PID_KI_Q16                 (8 << 16)
#endif
#ifndef PAPR_PID_KD_Q16
#define PAPR_PID_KD_Q16                 (2 << 16)
#endif

/* PWM resolution. 12-bit gives 0.024 % step, suitable for brushless drivers. */
#ifndef PAPR_PWM_MAX
#define PAPR_PWM_MAX                    4095U
#endif

/* Watchdog kick interval. Must be shorter than the hardware watchdog window. */
#ifndef PAPR_WDT_KICK_MS
#define PAPR_WDT_KICK_MS                100U
#endif

#endif /* PAPR_CONFIG_H */
