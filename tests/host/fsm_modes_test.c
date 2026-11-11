#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "fsm.h"
#include "global.h"
#include "lcd.h"
#include "mock_modules.h"
#include "software_timer.h"

static void expect_pair(traffic_color_t road1, traffic_color_t road2)
{
    assert(mock_traffic_road1() == road1 && mock_traffic_road2() == road2);
    assert(current_road1 == road1 && current_road2 == road2);
    assert(mock_traffic_dual_green_requests() == 0);
}

static void start_system(void)
{
    button_init();
    traffic_init();
    sw_timer_init();
    dht20_init();
    assert(lcd_init());
    fsm_init();
    assert(system_mode == SYSTEM_BOOT);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
}

static void advance(uint32_t ticks)
{
    for (uint32_t tick = 0; tick < ticks; ++tick) {
        sw_timer_tick();
        fsm_run();
        assert(mock_traffic_dual_green_requests() == 0);
    }
}

static void timer_only(uint32_t ticks)
{
    for (uint32_t tick = 0; tick < ticks; ++tick) {
        sw_timer_tick();
    }
}

static void inject(button_id_t button)
{
    mock_button_inject((button_event_t){ button, BUTTON_EVENT_PRESS });
}

static void press(button_id_t button)
{
    inject(button);
    fsm_run();
}

static void finish_switch(system_mode_t destination, bool has_amber)
{
    if (has_amber) {
        assert(system_mode == SYSTEM_SWITCH_AMBER);
        advance(199);
        assert(system_mode == SYSTEM_SWITCH_AMBER);
        advance(1);
    }
    assert(system_mode == SYSTEM_SWITCH_RED);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
    advance(99);
    assert(system_mode == SYSTEM_SWITCH_RED);
    /* A press at the completion boundary belongs to the old transition. */
    inject(BUTTON_D7);
    inject(BUTTON_D8);
    advance(1);
    assert(system_mode == destination);
    fsm_run();
    assert(system_mode == destination);
}

static void enter_manual(void)
{
    start_system();
    advance(100);
    press(BUTTON_D7);
    finish_switch(SYSTEM_MANUAL, true);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
}

static void enter_tuning(void)
{
    enter_manual();
    press(BUTTON_D7);
    finish_switch(SYSTEM_TUNING, false);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
}

static void test_switch_from_every_auto_phase(void)
{
    const uint32_t ticks[] = { 100, 600, 800, 900, 1400, 1600 };
    const traffic_color_t road1[] = {
        TRAFFIC_GREEN, TRAFFIC_AMBER, TRAFFIC_RED,
        TRAFFIC_RED, TRAFFIC_RED, TRAFFIC_RED
    };
    const traffic_color_t road2[] = {
        TRAFFIC_RED, TRAFFIC_RED, TRAFFIC_RED,
        TRAFFIC_GREEN, TRAFFIC_AMBER, TRAFFIC_RED
    };
    for (unsigned phase = 0; phase < 6; ++phase) {
        start_system();
        advance(ticks[phase]);
        expect_pair(road1[phase], road2[phase]);
        press(BUTTON_D7);
        const bool amber = road1[phase] != TRAFFIC_RED || road2[phase] != TRAFFIC_RED;
        if (amber) {
            if (road1[phase] != TRAFFIC_RED) {
                expect_pair(TRAFFIC_AMBER, TRAFFIC_RED);
            } else {
                expect_pair(TRAFFIC_RED, TRAFFIC_AMBER);
            }
        }
        finish_switch(SYSTEM_MANUAL, amber);
        expect_pair(TRAFFIC_RED, TRAFFIC_RED);
        press(BUTTON_D8);
        expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);
    }
}

static void test_switch_from_every_manual_phase(void)
{
    for (unsigned phase = 0; phase < 7; ++phase) {
        enter_manual();
        if (phase >= 1) { press(BUTTON_D8); }
        if (phase >= 2) { press(BUTTON_D8); }
        if (phase >= 3) { advance(200); }
        if (phase >= 4) { advance(100); }
        if (phase >= 5) { press(BUTTON_D8); }
        if (phase >= 6) { advance(200); }
        const bool amber = current_road1 != TRAFFIC_RED || current_road2 != TRAFFIC_RED;
        press(BUTTON_D7);
        finish_switch(SYSTEM_TUNING, amber);
        assert(strncmp(mock_lcd_line(0), "TUNE RED", 8) == 0);
    }
}

static void test_event_priority_and_stale_timeouts(void)
{
    start_system();
    advance(100);
    timer_only(500);
    inject(BUTTON_D8);
    inject(BUTTON_USER);
    inject(BUTTON_D7);
    fsm_run();
    assert(system_mode == SYSTEM_SWITCH_AMBER);
    finish_switch(SYSTEM_MANUAL, true);

    inject(BUTTON_D8);
    inject(BUTTON_D7);
    fsm_run();
    assert(system_mode == SYSTEM_SWITCH_RED);
    finish_switch(SYSTEM_TUNING, false);

    inject(BUTTON_D10);
    inject(BUTTON_D9);
    inject(BUTTON_D8);
    fsm_run();
    assert(strncmp(mock_lcd_line(0), "TUNE GREEN", 10) == 0);
    assert(strncmp(mock_lcd_line(1), "5 s", 3) == 0);
    mock_button_inject((button_event_t){ BUTTON_D7, BUTTON_EVENT_LONG_PRESS });
    fsm_run();
    assert(system_mode == SYSTEM_TUNING);

    enter_manual();
    press(BUTTON_D8);
    press(BUTTON_D8);
    timer_only(200);
    press(BUTTON_D7);
    finish_switch(SYSTEM_TUNING, true);
}

static void test_tuning_commit_and_reset(void)
{
    enter_tuning();
    press(BUTTON_D10);
    press(BUTTON_D7);
    assert(system_mode == SYSTEM_TUNING);
    assert(active_timing.red_seconds == 9);
    assert(strncmp(mock_lcd_line(1), "R < G+Y+2", 9) == 0);

    mock_dht20_sample(DHT20_OK, 300, 700);
    fsm_run();
    assert(strncmp(mock_lcd_line(1), "R < G+Y+2", 9) == 0);
    press(BUTTON_D9);
    press(BUTTON_D9);
    press(BUTTON_D9);
    press(BUTTON_D7);
    assert(system_mode == SYSTEM_SWITCH_RED);
    assert(active_timing.red_seconds == 9);
    advance(99);
    assert(active_timing.red_seconds == 9);
    advance(1);
    assert(system_mode == SYSTEM_AUTOMATIC);
    assert(active_timing.red_seconds == 11);
    expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);

    advance(500);
    expect_pair(TRAFFIC_AMBER, TRAFFIC_RED);
    advance(200);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
    advance(199);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
    advance(1);
    expect_pair(TRAFFIC_RED, TRAFFIC_GREEN);
    advance(500 + 200);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
    advance(200);
    expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);

    inject(BUTTON_D7);
    fsm_init();
    assert(active_timing.red_seconds == 9);
    advance(100);
    assert(system_mode == SYSTEM_AUTOMATIC);
    expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);
}

static void test_lcd_cache_and_retry(void)
{
    start_system();
    advance(100);
    const uint32_t writes = mock_lcd_write_requests();
    advance(99);
    assert(mock_lcd_write_requests() == writes);
    mock_dht20_sample(DHT20_OK, -5, 654);
    fsm_run();
    assert(strncmp(mock_lcd_line(1), "T:-0.5 H:65.4", 13) == 0);
    assert(mock_lcd_write_requests() == writes + 1);

    mock_fail_next_lcd_write();
    mock_dht20_sample(DHT20_ERROR, 0, 0);
    fsm_run();
    assert(strncmp(mock_lcd_line(1), "T:-0.5 H:65.4", 13) == 0);
    fsm_run();
    assert(strncmp(mock_lcd_line(1), "SENSOR ERROR", 12) == 0);
    const uint32_t after_retry = mock_lcd_write_requests();
    fsm_run();
    assert(mock_lcd_write_requests() == after_retry);
}

static void test_all_three_durations_and_late_switch(void)
{
    enter_tuning();
    press(BUTTON_D9);
    press(BUTTON_D8);
    press(BUTTON_D9);
    press(BUTTON_D8);
    press(BUTTON_D10);
    press(BUTTON_D7);
    advance(100);
    assert(active_timing.red_seconds == 10 && active_timing.green_seconds == 6
           && active_timing.amber_seconds == 1);
    advance(599);
    expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);
    advance(1);
    expect_pair(TRAFFIC_AMBER, TRAFFIC_RED);
    advance(100);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
    advance(149);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
    advance(1);
    expect_pair(TRAFFIC_RED, TRAFFIC_GREEN);
    advance(600);
    expect_pair(TRAFFIC_RED, TRAFFIC_AMBER);
    advance(100);
    expect_pair(TRAFFIC_RED, TRAFFIC_RED);
    advance(150);
    expect_pair(TRAFFIC_GREEN, TRAFFIC_RED);

    press(BUTTON_D7);
    timer_only(1000);
    fsm_run();
    assert(system_mode == SYSTEM_SWITCH_RED);
    advance(99);
    assert(system_mode == SYSTEM_SWITCH_RED);
    advance(1);
    assert(system_mode == SYSTEM_MANUAL);
}

static void test_failure_stops_until_reset(void)
{
    for (unsigned failure = 0; failure < 2; ++failure) {
        start_system();
        if (failure == 0) {
            mock_fail_next_timer_start();
        } else {
            mock_fail_next_traffic_pair();
        }
        advance(100);
        assert(system_mode == SYSTEM_FAULT);
        expect_pair(TRAFFIC_RED, TRAFFIC_RED);
        const uint32_t requests = mock_traffic_pair_requests();
        inject(BUTTON_D7);
        advance(1000);
        assert(system_mode == SYSTEM_FAULT);
        assert(mock_traffic_pair_requests() == requests);
        fsm_init();
        advance(100);
        assert(system_mode == SYSTEM_AUTOMATIC);
    }
}

static void test_mixed_events(void)
{
    start_system();
    uint32_t random = 0x13572468u;
    for (unsigned tick = 0; tick < 100000; ++tick) {
        random = random * 1664525u + 1013904223u;
        if ((random & 31u) == 0) {
            inject((button_id_t)((random >> 8) % BUTTON_COUNT));
        }
        if ((random & 4095u) == 1) {
            fsm_init();
        }
        advance(1);
        assert(traffic_timing_valid(&active_timing));
        assert(system_mode >= SYSTEM_BOOT && system_mode <= SYSTEM_FAULT);
        assert(current_road1 == mock_traffic_road1());
        assert(current_road2 == mock_traffic_road2());
    }
}

int main(void)
{
    test_switch_from_every_auto_phase();
    test_switch_from_every_manual_phase();
    puts("PASS: mode switching from all 6 Auto and 7 Manual phases");
    test_event_priority_and_stale_timeouts();
    test_tuning_commit_and_reset();
    test_all_three_durations_and_late_switch();
    puts("PASS: event arbitration, stale timeout cleanup and delayed config commit");
    test_lcd_cache_and_retry();
    test_failure_stops_until_reset();
    test_mixed_events();
    puts("PASS: LCD caching/retry, fail-stop recovery and 100000 mixed-event ticks");
    return 0;
}
