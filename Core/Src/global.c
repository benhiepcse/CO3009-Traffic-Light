#include "global.h"

#include <stddef.h>

#include "software_timer.h"

static const traffic_timing_t default_timing = {
    .red_seconds = 9U,
    .green_seconds = 5U,
    .amber_seconds = 2U
};

system_mode_t system_mode;
traffic_timing_t active_timing;
traffic_color_t current_road1;
traffic_color_t current_road2;

void global_reset(void)
{
    system_mode = SYSTEM_BOOT;
    active_timing = default_timing;
    current_road1 = TRAFFIC_RED;
    current_road2 = TRAFFIC_RED;
}

bool traffic_timing_valid(const traffic_timing_t *timing)
{
    if (timing == NULL) {
        return false;
    }

    if (timing->red_seconds < TRAFFIC_RED_MIN_SECONDS
        || timing->red_seconds > TRAFFIC_RED_MAX_SECONDS
        || timing->green_seconds < TRAFFIC_GREEN_MIN_SECONDS
        || timing->green_seconds > TRAFFIC_GREEN_MAX_SECONDS
        || timing->amber_seconds < TRAFFIC_AMBER_MIN_SECONDS
        || timing->amber_seconds > TRAFFIC_AMBER_MAX_SECONDS) {
        return false;
    }

    /* Allow at least one second for each all-red interval. */
    return timing->red_seconds
        >= timing->green_seconds + timing->amber_seconds + 2U;
}

uint32_t traffic_clearance_ticks(const traffic_timing_t *timing)
{
    if (!traffic_timing_valid(timing)) {
        return 0U;
    }

    const uint32_t total_clearance_seconds =
        timing->red_seconds - timing->green_seconds - timing->amber_seconds;

    return total_clearance_seconds * TRAFFIC_TICKS_PER_SECOND / 2U;
}

static bool apply_colors(traffic_color_t road1, traffic_color_t road2)
{
    if (!traffic_set_pair(road1, road2)) {
        return false;
    }

    current_road1 = road1;
    current_road2 = road2;
    return true;
}

static void enter_fault(void)
{
    system_mode = SYSTEM_FAULT;
    (void)sw_timer_cancel(SW_TIMER_TRAFFIC_PHASE);
    (void)apply_colors(TRAFFIC_RED, TRAFFIC_RED);
}

bool fsm_set_phase(traffic_color_t road1,
                   traffic_color_t road2,
                   uint32_t duration_ticks)
{
    if (!apply_colors(road1, road2)) {
        enter_fault();
        return false;
    }

    const bool timer_ok = duration_ticks == 0U
        ? sw_timer_cancel(SW_TIMER_TRAFFIC_PHASE)
        : sw_timer_start(SW_TIMER_TRAFFIC_PHASE, duration_ticks);

    if (!timer_ok) {
        enter_fault();
        return false;
    }

    return true;
}
