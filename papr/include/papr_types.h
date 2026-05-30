#ifndef PAPR_TYPES_H
#define PAPR_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum
{
    PAPR_OK = 0,
    PAPR_ERR_PARAM,
    PAPR_ERR_HW,
    PAPR_ERR_TIMEOUT,
    PAPR_ERR_SELF_TEST,
    PAPR_ERR_NOT_READY
} papr_status_t;

typedef enum
{
    PAPR_LEVEL_LOW = 0,
    PAPR_LEVEL_MED,
    PAPR_LEVEL_HIGH,
    PAPR_LEVEL_COUNT
} papr_flow_level_t;

typedef enum
{
    PAPR_STATE_INIT = 0,
    PAPR_STATE_SELF_TEST,
    PAPR_STATE_STANDBY,
    PAPR_STATE_RUNNING,
    PAPR_STATE_ALARM,
    PAPR_STATE_SHUTDOWN,
    PAPR_STATE_FAULT
} papr_state_t;

/* Bit mask. Multiple alarms may be active simultaneously. */
typedef enum
{
    PAPR_ALARM_NONE         = 0x00U,
    PAPR_ALARM_LOW_FLOW     = 0x01U,
    PAPR_ALARM_LOW_BATTERY  = 0x02U,
    PAPR_ALARM_CRIT_BATTERY = 0x04U,
    PAPR_ALARM_FILTER_CLOG  = 0x08U,
    PAPR_ALARM_MOTOR_FAULT  = 0x10U,
    PAPR_ALARM_SENSOR_FAULT = 0x20U,
    PAPR_ALARM_OVERTEMP     = 0x40U
} papr_alarm_t;

typedef struct
{
    uint16_t flow_lpm;            /* Measured airflow, litres per minute */
    int16_t  pressure_pa;         /* Differential pressure across filter (SDP810) */
    uint32_t absolute_pressure_pa;/* Ambient absolute pressure (GDY1124)  */
    int16_t  temperature_c10;     /* Motor housing temperature, 0.1 °C */
    uint16_t battery_mv;          /* Pack voltage, millivolts            */
    uint16_t battery_ma;          /* Pack discharge current, milliamps   */
    uint8_t  battery_soc_percent; /* State of charge, 0..100             */
    uint16_t motor_rpm;           /* Brushless motor speed               */
    uint16_t duty_permille;       /* Current PWM duty in 0.1 %           */
    uint8_t  breaths_per_min;     /* Detected breathing rate             */
    uint16_t remaining_minutes;   /* Estimated runtime at current draw   */
    uint8_t  auto_mode_active;    /* 0 = manual, 1 = adaptive comfort    */
} papr_telemetry_t;

#endif /* PAPR_TYPES_H */
