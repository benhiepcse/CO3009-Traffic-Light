#include "fsm_manual.h"

#include <stdint.h>

#include "global.h"
#include "software_timer.h"
#include "traffic_light.h"

typedef enum {
    MANUAL_WAIT,
    MANUAL_ROAD1_GREEN,
    MANUAL_ROAD1_AMBER,
    MANUAL_ALL_RED_BEFORE_ROAD2,
    MANUAL_ROAD2_GREEN,
    MANUAL_ROAD2_AMBER,
    MANUAL_ALL_RED_BEFORE_ROAD1
} manual_phase_t;

static manual_phase_t current_phase;

static void enter_phase(manual_phase_t phase,
                        traffic_color_t road1,
                        traffic_color_t road2,
                        uint32_t duration_ticks)
{
    if (fsm_set_phase(road1, road2, duration_ticks)) {
        current_phase = phase;
    }
}

static uint32_t amber_duration_ticks(void)
{
    return active_timing.amber_seconds * TRAFFIC_TICKS_PER_SECOND;
}

static void enter_clearance(manual_phase_t phase)
{
    const uint32_t ticks = traffic_clearance_ticks(&active_timing);

    if (ticks == 0U) {
        system_mode = SYSTEM_FAULT;
        (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, 0U);
        return;
    }

    enter_phase(phase, TRAFFIC_RED, TRAFFIC_RED, ticks);
}

void fsm_manual_enter(void)
{
    enter_phase(MANUAL_WAIT, TRAFFIC_RED, TRAFFIC_RED, 0U);
}

void fsm_manual_run(bool next_pressed)
{
    switch (current_phase) {
    case MANUAL_WAIT:
        if (next_pressed) {
            enter_phase(MANUAL_ROAD1_GREEN, TRAFFIC_GREEN, TRAFFIC_RED, 0U);
        }
        break;

    case MANUAL_ROAD1_GREEN:
        if (next_pressed) {
            enter_phase(MANUAL_ROAD1_AMBER, TRAFFIC_AMBER, TRAFFIC_RED,
                        amber_duration_ticks());
        }
        break;

    case MANUAL_ROAD1_AMBER:
        if (sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
            enter_clearance(MANUAL_ALL_RED_BEFORE_ROAD2);
        }
        break;

    case MANUAL_ALL_RED_BEFORE_ROAD2:
        if (sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
            enter_phase(MANUAL_ROAD2_GREEN, TRAFFIC_RED, TRAFFIC_GREEN, 0U);
        }
        break;

    case MANUAL_ROAD2_GREEN:
        if (next_pressed) {
            enter_phase(MANUAL_ROAD2_AMBER, TRAFFIC_RED, TRAFFIC_AMBER,
                        amber_duration_ticks());
        }
        break;

    case MANUAL_ROAD2_AMBER:
        if (sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
            enter_clearance(MANUAL_ALL_RED_BEFORE_ROAD1);
        }
        break;

    case MANUAL_ALL_RED_BEFORE_ROAD1:
        if (sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
            enter_phase(MANUAL_ROAD1_GREEN, TRAFFIC_GREEN, TRAFFIC_RED, 0U);
        }
        break;

    default:
        system_mode = SYSTEM_FAULT;
        (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, 0U);
        break;
    }
}
