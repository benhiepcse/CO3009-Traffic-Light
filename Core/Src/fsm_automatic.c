#include "fsm_automatic.h"

#include "global.h"
#include "software_timer.h"

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
    automatic_phase_t next;
} phase_step_t;

static const phase_step_t cycle[A_STOPPED] = {
    [A_G1] = { TRAFFIC_GREEN, TRAFFIC_RED, A_Y1 },
    [A_Y1] = { TRAFFIC_AMBER, TRAFFIC_RED, A_RR12 },
    [A_RR12] = { TRAFFIC_RED, TRAFFIC_RED, A_G2 },
    [A_G2] = { TRAFFIC_RED, TRAFFIC_GREEN, A_Y2 },
    [A_Y2] = { TRAFFIC_RED, TRAFFIC_AMBER, A_RR21 },
    [A_RR21] = { TRAFFIC_RED, TRAFFIC_RED, A_G1 }
};

static automatic_phase_t current_phase = A_STOPPED;

static void enter_phase(automatic_phase_t phase)
{
    const phase_step_t *step = &cycle[phase];
    uint32_t duration_ticks;
    if (phase == A_G1 || phase == A_G2) {
        duration_ticks = active_timing.green_seconds * TRAFFIC_TICKS_PER_SECOND;
    } else if (phase == A_Y1 || phase == A_Y2) {
        duration_ticks = active_timing.amber_seconds * TRAFFIC_TICKS_PER_SECOND;
    } else {
        duration_ticks = traffic_clearance_ticks(&active_timing);
    }

    if (fsm_set_phase(step->road1, step->road2, duration_ticks)) {
        current_phase = phase;
        return;
    }

    current_phase = A_STOPPED;
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
