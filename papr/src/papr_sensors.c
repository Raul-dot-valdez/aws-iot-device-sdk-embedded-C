#include "papr_sensors.h"
#include "papr_hal.h"

papr_status_t papr_sensors_init(papr_sensors_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }
    s->flow_lpm                  = 0U;
    s->pressure_pa               = 0;
    s->absolute_pressure_pa      = 0U;
    s->temperature_c10           = 0;
    s->flow_valid                = false;
    s->pressure_valid            = false;
    s->absolute_pressure_valid   = false;
    s->temperature_valid         = false;

    /* The bus drivers each issue chip-specific bring-up. Failure here is
     * non-fatal — the first read attempt will retry. */
    (void)papr_sdp810_init(&s->pressure_sensor);
    (void)papr_gdy1124_init(&s->baro_sensor);
    return PAPR_OK;
}

papr_status_t papr_sensors_update(papr_sensors_t *s)
{
    if (s == NULL) { return PAPR_ERR_PARAM; }

    uint16_t v16 = 0U;
    int16_t  v_s = 0;

    s->flow_valid        = (papr_hal_read_flow_lpm(&v16) == PAPR_OK);
    if (s->flow_valid)        { s->flow_lpm = v16; }

    s->temperature_valid = (papr_hal_read_temperature_c10(&v_s) == PAPR_OK);
    if (s->temperature_valid) { s->temperature_c10 = v_s; }

    papr_status_t pst = papr_sdp810_read(&s->pressure_sensor);
    s->pressure_valid  = (pst == PAPR_OK) &&
                         papr_sdp810_valid(&s->pressure_sensor);
    if (s->pressure_valid)
    {
        s->pressure_pa = papr_sdp810_pressure_pa(&s->pressure_sensor);
    }

    papr_status_t bst = papr_gdy1124_read(&s->baro_sensor);
    s->absolute_pressure_valid = (bst == PAPR_OK) &&
                                 papr_gdy1124_valid(&s->baro_sensor);
    if (s->absolute_pressure_valid)
    {
        s->absolute_pressure_pa = papr_gdy1124_pressure_pa(&s->baro_sensor);
    }

    return papr_sensors_all_valid(s) ? PAPR_OK : PAPR_ERR_HW;
}

bool papr_sensors_all_valid(const papr_sensors_t *s)
{
    if (s == NULL) { return false; }
    /* Absolute pressure is informational (telemetry only) so it is not
     * required for the supervisor to consider sensors healthy. */
    return s->flow_valid && s->pressure_valid && s->temperature_valid;
}
