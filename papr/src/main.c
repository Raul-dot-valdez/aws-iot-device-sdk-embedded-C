#include "papr_controller.h"

/* Bare-metal entry point. The MCU vendor's start-up code calls main() after
 * clock and stack initialisation. The HAL implementation handles peripheral
 * bring-up; this file simply drives the supervisor loop. */

int main(void)
{
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
