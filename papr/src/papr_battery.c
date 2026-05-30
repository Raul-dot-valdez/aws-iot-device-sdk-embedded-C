#include "papr_battery.h"
#include "papr_config.h"
#include "papr_hal.h"

static uint8_t voltage_to_soc(uint16_t mv)
{
    if (mv >= PAPR_BATT_FULL_MV) { return 100U; }
    if (mv <= PAPR_BATT_CUTOFF_MV) { return 0U; }
    uint32_t span = (uint32_t)(PAPR_BATT_FULL_MV - PAPR_BATT_CUTOFF_MV);
    uint32_t over = (uint32_t)(mv - PAPR_BATT_CUTOFF_MV);
    return (uint8_t)((over * 100U) / span);
}

static papr_batt_level_t classify(uint16_t mv)
{
    if (mv <= PAPR_BATT_CUTOFF_MV)   { return PAPR_BATT_CUTOFF; }
    if (mv <= PAPR_BATT_CRITICAL_MV) { return PAPR_BATT_CRITICAL; }
    if (mv <= PAPR_BATT_LOW_MV)      { return PAPR_BATT_LOW; }
    return PAPR_BATT_OK;
}

papr_status_t papr_battery_init(papr_battery_t *bat)
{
    if (bat == NULL) { return PAPR_ERR_PARAM; }
    bat->voltage_mv  = 0U;
    bat->current_ma  = 0U;
    bat->soc_percent = 0U;
    bat->level       = PAPR_BATT_OK;
    return PAPR_OK;
}

papr_status_t papr_battery_update(papr_battery_t *bat)
{
    if (bat == NULL) { return PAPR_ERR_PARAM; }

    uint16_t mv = 0U;
    uint16_t ma = 0U;
    papr_status_t s = papr_hal_read_battery_mv(&mv);
    if (s != PAPR_OK) { return s; }
    s = papr_hal_read_battery_ma(&ma);
    if (s != PAPR_OK) { return s; }

    /* Simple low-pass to suppress ADC ripple under motor load. */
    bat->voltage_mv = (uint16_t)(((uint32_t)bat->voltage_mv * 3U + mv) / 4U);
    bat->current_ma = (uint16_t)(((uint32_t)bat->current_ma * 3U + ma) / 4U);
    bat->soc_percent = voltage_to_soc(bat->voltage_mv);
    bat->level       = classify(bat->voltage_mv);
    return PAPR_OK;
}
