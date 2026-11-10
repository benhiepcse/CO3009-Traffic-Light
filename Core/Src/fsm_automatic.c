#include "fsm_automatic.h"

#include <stdint.h>

#include "software_timer.h"
#include "traffic_light.h"

enum {
    TICKS_PER_SECOND = 100,
    DEFAULT_GREEN_TICKS = 5 * TICKS_PER_SECOND,
    DEFAULT_AMBER_TICKS = 2 * TICKS_PER_SECOND,
    DEFAULT_RED_TICKS = 9 * TICKS_PER_SECOND,
    DEFAULT_ALL_RED_TICKS =
        (DEFAULT_RED_TICKS - DEFAULT_GREEN_TICKS - DEFAULT_AMBER_TICKS) / 2
};

typedef enum {
    A_G1,
    A_Y1,
    A_RR12,
    A_G2,
    A_Y2,
    A_RR21,
    A_STOPPED
} automatic_phase_t;

typedef struct {
    traffic_color_t road1;
    traffic_color_t road2;
    uint32_t duration_ticks;
    automatic_phase_t next;
} phase_step_t;

static const phase_step_t cycle[A_STOPPED] = {
    [A_G1] = { TRAFFIC_GREEN, TRAFFIC_RED, DEFAULT_GREEN_TICKS, A_Y1 },
    [A_Y1] = { TRAFFIC_AMBER, TRAFFIC_RED, DEFAULT_AMBER_TICKS, A_RR12 },
    [A_RR12] = { TRAFFIC_RED, TRAFFIC_RED, DEFAULT_ALL_RED_TICKS, A_G2 },
    [A_G2] = { TRAFFIC_RED, TRAFFIC_GREEN, DEFAULT_GREEN_TICKS, A_Y2 },
    [A_Y2] = { TRAFFIC_RED, TRAFFIC_AMBER, DEFAULT_AMBER_TICKS, A_RR21 },
    [A_RR21] = { TRAFFIC_RED, TRAFFIC_RED, DEFAULT_ALL_RED_TICKS, A_G1 }
};

static automatic_phase_t current_phase = A_STOPPED;

static void enter_phase(automatic_phase_t phase)
{
    const phase_step_t *step = &cycle[phase];

    if (traffic_set_pair(step->road1, step->road2)
        && sw_timer_start(SW_TIMER_TRAFFIC_PHASE, step->duration_ticks)) {
        current_phase = phase;
        return;
    }

    current_phase = A_STOPPED;
    (void)sw_timer_cancel(SW_TIMER_TRAFFIC_PHASE);
    (void)traffic_set_pair(TRAFFIC_RED, TRAFFIC_RED);
}

void fsm_automatic_enter(void)
{
    enter_phase(A_G1);
}

void fsm_automatic_run(void)
{
    if (current_phase != A_STOPPED
        && sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
        enter_phase(cycle[current_phase].next);
    }
}
