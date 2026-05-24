#include "papr_controller.h"
#include "papr_factory.h"
#include "papr_hal.h"

/* Bare-metal entry point. The MCU vendor's start-up code calls main() after
 * clock and stack initialisation. The HAL implementation handles peripheral
 * bring-up; this file picks the operating mode and drives the matching loop. */

int main(void)
{
    /* Production path: if the end-of-line test fixture asserts the TEST_MODE
     * pad, run the factory command loop instead of the application. Checked
     * before full init so a blank, unprovisioned board still enters test.
     * papr_hal_factory_requested() brings up only what it needs to read the
     * pad and is safe to call ahead of papr_hal_init(). */
    if (papr_hal_factory_requested())
    {
        (void)papr_hal_init();
        papr_factory_main();   /* no return on real hardware (FCT_REBOOT) */
    }

    static papr_controller_t controller;

    if (papr_controller_init(&controller) != PAPR_OK)
    {
        for (;;) { /* fatal: stay halted, let the watchdog reset */ }
    }

    for (;;)
    {
        (void)papr_controller_step(&controller);
    }
}
