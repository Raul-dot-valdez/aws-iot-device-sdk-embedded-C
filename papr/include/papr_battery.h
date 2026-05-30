#ifndef PAPR_BATTERY_H
#define PAPR_BATTERY_H

#include "papr_types.h"

typedef enum
{
    PAPR_BATT_OK = 0,
    PAPR_BATT_LOW,
    PAPR_BATT_CRITICAL,
    PAPR_BATT_CUTOFF
} papr_batt_level_t;

typedef struct
{
    uint16_t voltage_mv;
    uint16_t current_ma;
    uint8_t  soc_percent;       /* State of charge, 0..100 */
    papr_batt_level_t level;
} papr_battery_t;

papr_status_t papr_battery_init(papr_battery_t *bat);
papr_status_t papr_battery_update(papr_battery_t *bat);

#endif /* PAPR_BATTERY_H */
