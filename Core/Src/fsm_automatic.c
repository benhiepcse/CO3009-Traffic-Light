#include "fsm_automatic.h"

#include <stdint.h>

#include "global.h"
#include "software_timer.h"
#include "traffic_light.h"

typedef enum {
    AUTO_ROAD1_GREEN,
    AUTO_ROAD1_AMBER,
    AUTO_ALL_RED_BEFORE_ROAD2,
    AUTO_ROAD2_GREEN,
    AUTO_ROAD2_AMBER,
    AUTO_ALL_RED_BEFORE_ROAD1,
    AUTO_PHASE_COUNT
} automatic_phase_t;

typedef struct {
    traffic_color_t road1;
    traffic_color_t road2;
} automatic_outputs_t;

static const automatic_outputs_t phase_outputs[AUTO_PHASE_COUNT] = {
    [AUTO_ROAD1_GREEN]           = { TRAFFIC_GREEN, TRAFFIC_RED },
    [AUTO_ROAD1_AMBER]           = { TRAFFIC_AMBER, TRAFFIC_RED },
    [AUTO_ALL_RED_BEFORE_ROAD2]  = { TRAFFIC_RED,   TRAFFIC_RED },
    [AUTO_ROAD2_GREEN]           = { TRAFFIC_RED,   TRAFFIC_GREEN },
    [AUTO_ROAD2_AMBER]           = { TRAFFIC_RED,   TRAFFIC_AMBER },
    [AUTO_ALL_RED_BEFORE_ROAD1]  = { TRAFFIC_RED,   TRAFFIC_RED }
};

static automatic_phase_t current_phase;
static uint32_t green_ticks;
static uint32_t amber_ticks;
static uint32_t clearance_ticks;

static void enter_phase(automatic_phase_t phase)
{
    uint32_t duration_ticks;

    switch (phase) {
    case AUTO_ROAD1_GREEN:
    case AUTO_ROAD2_GREEN:
        duration_ticks = green_ticks;
        break;

    case AUTO_ROAD1_AMBER:
    case AUTO_ROAD2_AMBER:
        duration_ticks = amber_ticks;
        break;

    case AUTO_ALL_RED_BEFORE_ROAD2:
    case AUTO_ALL_RED_BEFORE_ROAD1:
        duration_ticks = clearance_ticks;
        break;

    default:
        system_mode = SYSTEM_FAULT;
        (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, 0U);
        return;
    }

    const automatic_outputs_t *outputs = &phase_outputs[phase];
    current_phase = phase;
    (void)fsm_set_phase(outputs->road1, outputs->road2, duration_ticks);
}

void fsm_automatic_enter(void)
{
    clearance_ticks = traffic_clearance_ticks(&active_timing);

    if (clearance_ticks == 0U) {
        system_mode = SYSTEM_FAULT;
        (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, 0U);
        return;
    }

    green_ticks = active_timing.green_seconds * TRAFFIC_TICKS_PER_SECOND;
    amber_ticks = active_timing.amber_seconds * TRAFFIC_TICKS_PER_SECOND;

    enter_phase(AUTO_ROAD1_GREEN);
}

void fsm_automatic_run(void)
{
    if (!sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
        return;
    }

    if (current_phase == AUTO_ALL_RED_BEFORE_ROAD1) {
        enter_phase(AUTO_ROAD1_GREEN);
    } else {
        enter_phase((automatic_phase_t)(current_phase + 1));
    }
}
