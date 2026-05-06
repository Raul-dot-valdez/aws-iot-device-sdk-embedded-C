#include "papr_l6235.h"
#include "papr_config.h"
#include "papr_hal.h"

/* Map a current setpoint in milliamps to the L6235 VREF voltage in millivolts.
 *   I_peak = VREF / RSENSE   (L6235 sense amplifier has unity gain)
 * VREF[mV] = I[mA] * RSENSE[mΩ] / 1000
 *
 * Then translate VREF mV to a DAC code in 0..PAPR_L6235_VREF_DAC_MAX, where
 * the DAC's full-scale corresponds to PAPR_L6235_VREF_MAX_MV. */
static uint16_t current_ma_to_dac(uint16_t ma)
{
    uint32_t vref_mv = ((uint32_t)ma * PAPR_L6235_RSENSE_MOHM) / 1000U;
    if (vref_mv > PAPR_L6235_VREF_MAX_MV) { vref_mv = PAPR_L6235_VREF_MAX_MV; }
    uint32_t code = (vref_mv * PAPR_L6235_VREF_DAC_MAX) / PAPR_L6235_VREF_MAX_MV;
    if (code > PAPR_L6235_VREF_DAC_MAX) { code = PAPR_L6235_VREF_DAC_MAX; }
    return (uint16_t)code;
}

papr_status_t papr_l6235_init(papr_l6235_t *drv)
{
    if (drv == NULL) { return PAPR_ERR_PARAM; }
    drv->commanded_ma  = 0U;
    drv->measured_rpm  = 0U;
    drv->enabled       = false;
    drv->forward       = true;
    drv->fault_latched = false;

    papr_status_t s = papr_hal_l6235_set_enable(false);
    if (s != PAPR_OK) { return s; }
    s = papr_hal_l6235_set_brake(true);
    if (s != PAPR_OK) { return s; }
    s = papr_hal_l6235_set_forward(true);
    if (s != PAPR_OK) { return s; }
    return papr_hal_l6235_set_vref(0U);
}

papr_status_t papr_l6235_start(papr_l6235_t *drv)
{
    if (drv == NULL) { return PAPR_ERR_PARAM; }
    if (drv->fault_latched) { return PAPR_ERR_NOT_READY; }

    papr_status_t s = papr_hal_l6235_set_vref(0U);
    if (s != PAPR_OK) { return s; }
    s = papr_hal_l6235_set_forward(true);
    if (s != PAPR_OK) { return s; }
    s = papr_hal_l6235_set_brake(false);
    if (s != PAPR_OK) { return s; }
    s = papr_hal_l6235_set_enable(true);
    if (s != PAPR_OK) { return s; }

    drv->forward      = true;
    drv->enabled      = true;
    drv->commanded_ma = 0U;
    return PAPR_OK;
}

papr_status_t papr_l6235_stop(papr_l6235_t *drv)
{
    if (drv == NULL) { return PAPR_ERR_PARAM; }
    (void)papr_hal_l6235_set_vref(0U);
    (void)papr_hal_l6235_set_enable(false);
    (void)papr_hal_l6235_set_brake(true);
    drv->commanded_ma = 0U;
    drv->enabled      = false;
    return PAPR_OK;
}

papr_status_t papr_l6235_set_current_ma(papr_l6235_t *drv, uint16_t ma)
{
    if (drv == NULL) { return PAPR_ERR_PARAM; }
    if (!drv->enabled || drv->fault_latched)
    {
        drv->commanded_ma = 0U;
        return papr_hal_l6235_set_vref(0U);
    }

    if (ma > PAPR_L6235_IMAX_MA) { ma = PAPR_L6235_IMAX_MA; }
    if (ma > 0U && ma < PAPR_L6235_IMIN_MA) { ma = PAPR_L6235_IMIN_MA; }

    drv->commanded_ma = ma;
    return papr_hal_l6235_set_vref(current_ma_to_dac(ma));
}

papr_status_t papr_l6235_poll(papr_l6235_t *drv)
{
    if (drv == NULL) { return PAPR_ERR_PARAM; }

    if (papr_hal_l6235_diag_active())
    {
        drv->fault_latched = true;
        (void)papr_hal_l6235_set_vref(0U);
        (void)papr_hal_l6235_set_enable(false);
        drv->enabled       = false;
        drv->commanded_ma  = 0U;
    }

    uint16_t rpm = 0U;
    if (papr_hal_l6235_read_tacho_rpm(&rpm) == PAPR_OK)
    {
        drv->measured_rpm = rpm;
    }
    return PAPR_OK;
}

bool papr_l6235_has_fault(const papr_l6235_t *drv)
{
    return (drv != NULL) && drv->fault_latched;
}

void papr_l6235_clear_fault(papr_l6235_t *drv)
{
    if (drv != NULL) { drv->fault_latched = false; }
}

uint16_t papr_l6235_rpm(const papr_l6235_t *drv)
{
    return (drv == NULL) ? 0U : drv->measured_rpm;
}

uint16_t papr_l6235_demand_permille(const papr_l6235_t *drv)
{
    if (drv == NULL || PAPR_L6235_IMAX_MA == 0U) { return 0U; }
    uint32_t scaled = ((uint32_t)drv->commanded_ma * 1000U) / PAPR_L6235_IMAX_MA;
    if (scaled > 1000U) { scaled = 1000U; }
    return (uint16_t)scaled;
}
