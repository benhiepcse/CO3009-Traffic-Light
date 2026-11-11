#include "fsm_manual.h"

#include "global.h"
#include "software_timer.h"

typedef enum {
    M_WAIT,
    M_G1,
    M_Y1,
    M_RR12,
    M_G2,
    M_Y2,
    M_RR21
} manual_phase_t;

typedef struct {
    traffic_color_t road1;
    traffic_color_t road2;
    manual_phase_t next;
} manual_step_t;

static const manual_step_t steps[] = {
    [M_WAIT] = { TRAFFIC_RED, TRAFFIC_RED, M_G1 },
    [M_G1] = { TRAFFIC_GREEN, TRAFFIC_RED, M_Y1 },
    [M_Y1] = { TRAFFIC_AMBER, TRAFFIC_RED, M_RR12 },
    [M_RR12] = { TRAFFIC_RED, TRAFFIC_RED, M_G2 },
    [M_G2] = { TRAFFIC_RED, TRAFFIC_GREEN, M_Y2 },
    [M_Y2] = { TRAFFIC_RED, TRAFFIC_AMBER, M_RR21 },
    [M_RR21] = { TRAFFIC_RED, TRAFFIC_RED, M_G1 }
};

static manual_phase_t current_phase;

static void enter_phase(manual_phase_t phase)
{
    uint32_t duration_ticks = 0;
    if (phase == M_Y1 || phase == M_Y2) {
        duration_ticks = active_timing.amber_seconds * TRAFFIC_TICKS_PER_SECOND;
    } else if (phase == M_RR12 || phase == M_RR21) {
        duration_ticks = traffic_clearance_ticks(&active_timing);
    }

    const manual_step_t *step = &steps[phase];
    if (fsm_set_phase(step->road1, step->road2, duration_ticks)) {
        current_phase = phase;
    }
}

void fsm_manual_enter(void)
{
    enter_phase(M_WAIT);
}

void fsm_manual_run(bool next_requested)
{
    switch (current_phase) {
    case M_WAIT:
    case M_G1:
    case M_G2:
        if (next_requested) {
            enter_phase(steps[current_phase].next);
        }
        break;
    default:
        if (sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
            enter_phase(steps[current_phase].next);
        }
        break;
    }
}
