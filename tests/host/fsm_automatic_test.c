#include <assert.h>
#include <stdio.h>

#include "fsm.h"
#include "mock_modules.h"
#include "software_timer.h"

static void start_system(void)
{
    button_init();
    traffic_init();
    sw_timer_init();
    fsm_init();
}

static void expect_pair(traffic_color_t road1, traffic_color_t road2)
{
    assert(mock_traffic_road1() == road1);
    assert(mock_traffic_road2() == road2);
    assert(mock_traffic_dual_green_requests() == 0);
}

/* Check each tick before expiry and require exactly one output request at expiry. */
static void expect_phase(traffic_color_t road1, traffic_color_t road2,
                         uint32_t duration_ticks)
{
    const uint32_t initial_requests = mock_traffic_pair_requests();

    for (uint32_t tick = 0; tick < duration_ticks; ++tick) {
        expect_pair(road1, road2);
        assert(mock_traffic_pair_requests() == initial_requests);
        sw_timer_tick();
        fsm_run();
    }

    assert(mock_traffic_pair_requests() == initial_requests + 1);
}

static void test_boot_and_three_cycles(void)
{
    start_system();
    expect_phase(TRAFFIC_RED, TRAFFIC_RED, 100);

    for (unsigned cycle = 0; cycle < 3; ++cycle) {
        expect_phase(TRAFFIC_GREEN, TRAFFIC_RED, 500);
        expect_phase(TRAFFIC_AMBER, TRAFFIC_RED, 200);
        expect_phase(TRAFFIC_RED, TRAFFIC_RED, 100);
        expect_phase(TRAFFIC_RED, TRAFFIC_GREEN, 500);
        expect_phase(TRAFFIC_RED, TRAFFIC_AMBER, 200);
        expect_phase(TRAFFIC_RED, TRAFFIC_RED, 100);
    }

    expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);
}

static void advance_timer_only(uint32_t ticks)
{
    for (uint32_t tick = 0; tick < ticks; ++tick) {
        sw_timer_tick();
    }
}

static void reset_with_pending_event(void)
{
    const button_event_t press = { BUTTON_D7, BUTTON_EVENT_PRESS };
    button_event_t event;

    mock_button_inject(press);
    fsm_init();
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
    assert(!button_take_event(&event));

    mock_button_inject(press);
    fsm_run();
    assert(!button_take_event(&event));
    expect_phase(TRAFFIC_RED, TRAFFIC_RED, 100);
    expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);
}

static void test_reset_from_green_and_amber(void)
{
    start_system();
    expect_phase(TRAFFIC_RED, TRAFFIC_RED, 100);
    advance_timer_only(500);
    expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);
    reset_with_pending_event();

    expect_phase(TRAFFIC_GREEN, TRAFFIC_RED, 500);
    advance_timer_only(200);
    expect_pair(TRAFFIC_AMBER, TRAFFIC_RED);
    reset_with_pending_event();

    expect_phase(TRAFFIC_GREEN, TRAFFIC_RED, 500);
    expect_pair(TRAFFIC_AMBER, TRAFFIC_RED);
}

static void test_delayed_dispatch_keeps_clearance(void)
{
    start_system();
    expect_phase(TRAFFIC_RED, TRAFFIC_RED, 100);
    advance_timer_only(850);
    expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);

    fsm_run();
    expect_phase(TRAFFIC_AMBER, TRAFFIC_RED, 200);
    expect_phase(TRAFFIC_RED, TRAFFIC_RED, 100);
    expect_pair(TRAFFIC_RED, TRAFFIC_GREEN);
}

int main(void)
{
    test_boot_and_three_cycles();
    puts("PASS: boot and three 1600-tick cycles; no dual-green requests");
    test_reset_from_green_and_amber();
    puts("PASS: reset from green/amber clears stale timeout and button events");
    test_delayed_dispatch_keeps_clearance();
    puts("PASS: late dispatch preserves full amber and all-red intervals");
    return 0;
}
