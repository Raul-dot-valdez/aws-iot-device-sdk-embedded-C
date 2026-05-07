#ifndef PAPR_SENSORS_H
#define PAPR_SENSORS_H

#include "papr_gdy1124.h"
#include "papr_sdp810.h"
#include "papr_types.h"

/* Environmental sensors:
 *   - flow            : analog flow tube → ADC (HAL)
 *   - temperature     : NTC on motor housing → ADC (HAL)
 *   - dP across filter: Sensirion SDP810-500Pa over I²C (papr_sdp810)
 *   - ambient pressure: GigaDevice GDY1124 over I²C (papr_gdy1124)
 *
 * Motor speed is sourced from the L6235 TACHO pin and lives in the blower /
 * driver layer rather than here. */

typedef struct
{
    uint16_t       flow_lpm;
    int16_t        pressure_pa;
    uint32_t       absolute_pressure_pa;
    int16_t        temperature_c10;
    bool           flow_valid;
    bool           pressure_valid;
    bool           absolute_pressure_valid;
    bool           temperature_valid;
    papr_sdp810_t  pressure_sensor;
    papr_gdy1124_t baro_sensor;
} papr_sensors_t;

papr_status_t papr_sensors_init(papr_sensors_t *s);
papr_status_t papr_sensors_update(papr_sensors_t *s);
bool papr_sensors_all_valid(const papr_sensors_t *s);

#endif /* PAPR_SENSORS_H */
