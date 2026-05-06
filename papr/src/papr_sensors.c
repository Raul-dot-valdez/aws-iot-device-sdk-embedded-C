#include "papr_sensors.h"
#include "papr_hal.h"

papr_status_t papr_sensors_init(papr_sensors_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    s->flow_lpm          = 0U;
    s->pressure_pa       = 0U;
    s->temperature_c10   = 0;
    s->flow_valid        = false;
    s->pressure_valid    = false;
    s->temperature_valid = false;
    return PAPR_OK;
}

papr_status_t papr_sensors_update(papr_sensors_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }

    uint16_t v16 = 0U;
    int16_t  v_s = 0;

    s->flow_valid        = (papr_hal_read_flow_lpm(&v16)        == PAPR_OK);
    if (s->flow_valid)        { s->flow_lpm = v16; }

    s->pressure_valid    = (papr_hal_read_pressure_pa(&v16)     == PAPR_OK);
    if (s->pressure_valid)    { s->pressure_pa = v16; }

    s->temperature_valid = (papr_hal_read_temperature_c10(&v_s) == PAPR_OK);
    if (s->temperature_valid) { s->temperature_c10 = v_s; }

    return papr_sensors_all_valid(s) ? PAPR_OK : PAPR_ERR_HW;
}

bool papr_sensors_all_valid(const papr_sensors_t *s)
{
    if (s == NULL) { return false; }
    return s->flow_valid && s->pressure_valid && s->temperature_valid;
}
