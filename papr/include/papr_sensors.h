#ifndef PAPR_SENSORS_H
#define PAPR_SENSORS_H

#include "papr_types.h"

typedef struct
{
    uint16_t flow_lpm;
    uint16_t pressure_pa;
    int16_t  temperature_c10;
    uint16_t motor_rpm;
    bool     flow_valid;
    bool     pressure_valid;
    bool     temperature_valid;
    bool     rpm_valid;
} papr_sensors_t;

papr_status_t papr_sensors_init(papr_sensors_t *s);
papr_status_t papr_sensors_update(papr_sensors_t *s);
bool papr_sensors_all_valid(const papr_sensors_t *s);

#endif /* PAPR_SENSORS_H */
