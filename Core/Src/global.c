#include "global.h"

#include <stddef.h>

#include "software_timer.h"

system_mode_t system_mode = SYSTEM_BOOT;
traffic_timing_t active_timing = { 9, 5, 2 };
traffic_color_t current_road1 = TRAFFIC_RED;
traffic_color_t current_road2 = TRAFFIC_RED;

void global_reset(void)
{
    system_mode = SYSTEM_BOOT;
    active_timing = (traffic_timing_t){ 9, 5, 2 };
    current_road1 = TRAFFIC_RED;
    current_road2 = TRAFFIC_RED;
}

bool traffic_timing_valid(const traffic_timing_t *timing)
{
    return timing != NULL
        && timing->red_seconds >= 5 && timing->red_seconds <= 60
        && timing->green_seconds >= 2 && timing->green_seconds <= 30
        && timing->amber_seconds >= 1 && timing->amber_seconds <= 5
        && timing->red_seconds >= timing->green_seconds + timing->amber_seconds + 2;
}

uint32_t traffic_clearance_ticks(const traffic_timing_t *timing)
{
    if (!traffic_timing_valid(timing)) {
        return 0;
    }
    return (timing->red_seconds - timing->green_seconds - timing->amber_seconds)
        * TRAFFIC_TICKS_PER_SECOND / 2;
}

bool fsm_set_phase(traffic_color_t road1, traffic_color_t road2,
                   uint32_t duration_ticks)
{
    if (traffic_set_pair(road1, road2)) {
        current_road1 = road1;
        current_road2 = road2;
        bool timer_ok;
        if (duration_ticks == 0) {
            timer_ok = sw_timer_cancel(SW_TIMER_TRAFFIC_PHASE);
        } else {
            timer_ok = sw_timer_start(SW_TIMER_TRAFFIC_PHASE, duration_ticks);
        }
        if (timer_ok) {
            return true;
        }
    }

    system_mode = SYSTEM_FAULT;
    (void)sw_timer_cancel(SW_TIMER_TRAFFIC_PHASE);
    if (traffic_set_pair(TRAFFIC_RED, TRAFFIC_RED)) {
        current_road1 = TRAFFIC_RED;
        current_road2 = TRAFFIC_RED;
    }
    return false;
}
