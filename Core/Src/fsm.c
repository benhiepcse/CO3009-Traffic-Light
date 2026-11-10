#include "fsm.h"

#include <stdbool.h>

#include "button.h"
#include "fsm_automatic.h"
#include "software_timer.h"
#include "traffic_light.h"

enum { BOOT_ALL_RED_TICKS = 100 };

static bool booting;

void fsm_init(void)
{
    booting = true;
    (void)sw_timer_cancel(SW_TIMER_TRAFFIC_PHASE);
    button_clear_events();

    if (traffic_set_pair(TRAFFIC_RED, TRAFFIC_RED)) {
        (void)sw_timer_start(SW_TIMER_TRAFFIC_PHASE, BOOT_ALL_RED_TICKS);
    }
}

void fsm_run(void)
{
    /* Button actions are added by the mode-switching FSM. */
    button_clear_events();

    if (booting) {
        if (sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
            booting = false;
            fsm_automatic_enter();
        }
        return;
    }

    fsm_automatic_run();
}
