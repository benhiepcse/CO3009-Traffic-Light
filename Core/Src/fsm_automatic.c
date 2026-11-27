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
    traffic_color_t colors[2];
    automatic_phase_t next;
} phase_step_t;

static const phase_step_t cycle[A_STOPPED] = {
    [A_G1] = { { TRAFFIC_GREEN, TRAFFIC_RED }, A_Y1 },
    [A_Y1] = { { TRAFFIC_AMBER, TRAFFIC_RED }, A_RR12 },
    [A_RR12] = { { TRAFFIC_RED, TRAFFIC_RED }, A_G2 },
    [A_G2] = { { TRAFFIC_RED, TRAFFIC_GREEN }, A_Y2 },
    [A_Y2] = { { TRAFFIC_RED, TRAFFIC_AMBER }, A_RR21 },
    [A_RR21] = { { TRAFFIC_RED, TRAFFIC_RED }, A_G1 }
};

static automatic_phase_t current_phase = A_STOPPED;

static uint32_t phase_ticks(automatic_phase_t phase)
{
    if (phase == A_G1 || phase == A_G2) {
        return active_timing.green_seconds * TRAFFIC_TICKS_PER_SECOND;
    }
    if (phase == A_Y1 || phase == A_Y2) {
        return active_timing.amber_seconds * TRAFFIC_TICKS_PER_SECOND;
    }
    return traffic_clearance_ticks(&active_timing);
}

static uint8_t seconds_left(unsigned road)
{
    if (current_phase == A_STOPPED) {
        return 0U;
    }
    uint32_t ticks = sw_timer_remaining_ticks(SW_TIMER_TRAFFIC_PHASE);
    /* Include all consecutive phases which keep this road the same color. */
    for (automatic_phase_t next = cycle[current_phase].next;
         cycle[next].colors[road] == cycle[current_phase].colors[road]; next = cycle[next].next) {
        ticks += phase_ticks(next);
    }
    return (uint8_t)((ticks + TRAFFIC_TICKS_PER_SECOND - 1U) / TRAFFIC_TICKS_PER_SECOND);
}

uint8_t fsm_automatic_road1_seconds_left(void)
{
    return seconds_left(0U);
}

uint8_t fsm_automatic_road2_seconds_left(void)
{
    return seconds_left(1U);
}

static void enter_phase(automatic_phase_t phase)
{
    const phase_step_t *step = &cycle[phase];
    if (fsm_set_phase(step->colors[0], step->colors[1], phase_ticks(phase))) {
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
