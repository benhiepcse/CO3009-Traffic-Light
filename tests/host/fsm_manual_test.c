#include <assert.h>
#include <stdio.h>

#include "fsm_manual.h"
#include "global.h"
#include "mock_modules.h"
#include "software_timer.h"

static void expect_pair(traffic_color_t road1, traffic_color_t road2)
{
    assert(mock_traffic_road1() == road1);
    assert(mock_traffic_road2() == road2);
    assert(current_road1 == road1 && current_road2 == road2);
    assert(mock_traffic_dual_green_requests() == 0);
}

static void hold_phase(traffic_color_t road1, traffic_color_t road2,
                       uint32_t ticks, bool press_next)
{
    const uint32_t requests = mock_traffic_pair_requests();
    for (uint32_t tick = 0; tick < ticks; ++tick) {
        expect_pair(road1, road2);
        assert(mock_traffic_pair_requests() == requests);
        sw_timer_tick();
        fsm_manual_run(press_next);
    }
}

int main(void)
{
    global_reset();
    traffic_init();
    sw_timer_init();
    system_mode = SYSTEM_MANUAL;
    fsm_manual_enter();
    hold_phase(TRAFFIC_RED, TRAFFIC_RED, 1000, false);

    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        fsm_manual_run(true);
        hold_phase(TRAFFIC_GREEN, TRAFFIC_RED, 1000, false);
        fsm_manual_run(true);
        hold_phase(TRAFFIC_AMBER, TRAFFIC_RED, 200, true);
        hold_phase(TRAFFIC_RED, TRAFFIC_RED, 100, true);
        hold_phase(TRAFFIC_RED, TRAFFIC_GREEN, 1000, false);
        fsm_manual_run(true);
        hold_phase(TRAFFIC_RED, TRAFFIC_AMBER, 200, true);
        hold_phase(TRAFFIC_RED, TRAFFIC_RED, 100, true);
        expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);
        /* Return to WAIT to check the entry timer is canceled each time. */
        fsm_manual_enter();
    }

    assert(system_mode == SYSTEM_MANUAL);
    puts("PASS: Manual waits for NEXT and preserves both yellow/all-red intervals");
    return 0;
}
