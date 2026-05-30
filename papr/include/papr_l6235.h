#ifndef PAPR_L6235_H
#define PAPR_L6235_H

#include "papr_types.h"

/* Driver for the ST L6235 three-phase brushless DC driver. The chip handles
 * Hall decoding and synchronous current chopping internally; the MCU only
 * needs to:
 *   - set a current reference via VREF (controls torque, hence flow)
 *   - hold EN high during operation
 *   - keep direction = forward and brake released for a PAPR blower
 *   - read DIAG to detect overcurrent or thermal shutdown
 *   - read TACHO to estimate motor RPM
 *
 * All current values are expressed in milliamps at the motor phase peak. */

typedef struct
{
    uint16_t commanded_ma;      /* Last current setpoint sent to the L6235  */
    uint16_t measured_rpm;      /* Last RPM read from TACHO                 */
    bool     enabled;
    bool     forward;
    bool     fault_latched;     /* Latches DIAG = active until cleared      */
} papr_l6235_t;

papr_status_t papr_l6235_init(papr_l6235_t *drv);

/* Energise the bridge and release the brake. Direction is forced forward. */
papr_status_t papr_l6235_start(papr_l6235_t *drv);

/* Cuts current to zero, disables the bridge, then engages the brake. */
papr_status_t papr_l6235_stop(papr_l6235_t *drv);

/* Sets the peak phase current. Values above PAPR_L6235_IMAX_MA are clamped. */
papr_status_t papr_l6235_set_current_ma(papr_l6235_t *drv, uint16_t ma);

/* Polls DIAG and refreshes RPM via the HAL. Call once per supervisor tick. */
papr_status_t papr_l6235_poll(papr_l6235_t *drv);

/* True if a fault is currently latched. */
bool papr_l6235_has_fault(const papr_l6235_t *drv);

/* Clears the latched-fault flag (typically after the user toggles power). */
void papr_l6235_clear_fault(papr_l6235_t *drv);

uint16_t papr_l6235_rpm(const papr_l6235_t *drv);

/* Returns the commanded current as a fraction of IMAX in tenths of a percent.
 * Used by the alarm subsystem to detect filter clogging (controller saturated
 * at maximum current = blower demanding full torque). */
uint16_t papr_l6235_demand_permille(const papr_l6235_t *drv);

#endif /* PAPR_L6235_H */
