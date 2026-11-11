#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "fsm_tuning.h"
#include "mock_modules.h"
#include "software_timer.h"

static traffic_timing_t accepted;

static void press_many(button_id_t button, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) {
        assert(!fsm_tuning_run(button, &accepted));
    }
}

static void expect_value(const char *title, const char *value)
{
    char lines[FSM_LCD_ROWS][FSM_LCD_COLUMNS + 1];
    fsm_tuning_format(lines);
    assert(strcmp(lines[0], title) == 0);
    assert(strcmp(lines[1], value) == 0);
    assert(active_timing.red_seconds == 9);
}

static void test_validation(void)
{
    assert(!traffic_timing_valid(NULL));
    assert(traffic_clearance_ticks(NULL) == 0);
    traffic_timing_t invalid = { UINT32_MAX, UINT32_MAX, UINT32_MAX };
    assert(!traffic_timing_valid(&invalid));
    assert(traffic_clearance_ticks(&invalid) == 0);

    unsigned valid_count = 0;
    for (uint32_t r = 4; r <= 61; ++r) {
        for (uint32_t g = 1; g <= 31; ++g) {
            for (uint32_t y = 0; y <= 6; ++y) {
                const traffic_timing_t timing = { r, g, y };
                const bool valid = r >= 5 && r <= 60 && g >= 2 && g <= 30
                    && y >= 1 && y <= 5 && r >= g + y + 2;
                assert(traffic_timing_valid(&timing) == valid);
                if (valid) {
                    ++valid_count;
                    assert(traffic_clearance_ticks(&timing) == (r - g - y) * 50);
                } else {
                    assert(traffic_clearance_ticks(&timing) == 0);
                }
            }
        }
    }
    assert(valid_count == 5800);
}

static void test_editing(void)
{
    global_reset();
    sw_timer_init();
    traffic_init();
    fsm_tuning_enter();
    expect_value("TUNE RED", "9 s");
    accepted = (traffic_timing_t){ 60, 30, 5 };
    press_many(BUTTON_D10, 1);
    assert(!fsm_tuning_run(BUTTON_D7, &accepted));
    expect_value("TUNE RED", "R < G+Y+2");
    assert(accepted.red_seconds == 60);
    press_many(BUTTON_D9, 1);
    assert(fsm_tuning_run(BUTTON_D7, &accepted));
    assert(accepted.red_seconds == 9);

    const char *titles[] = { "TUNE RED", "TUNE GREEN", "TUNE AMBER" };
    const char *maxima[] = { "60 s", "30 s", "5 s" };
    const char *minima[] = { "5 s", "2 s", "1 s" };
    for (unsigned field = 0; field < 3; ++field) {
        press_many(BUTTON_D9, 100);
        expect_value(titles[field], maxima[field]);
        press_many(BUTTON_D10, 100);
        expect_value(titles[field], minima[field]);
        press_many(BUTTON_D8, 1);
    }
    expect_value("TUNE RED", "5 s");
    assert(fsm_tuning_run(BUTTON_D7, &accepted));
    assert(accepted.red_seconds == 5 && accepted.green_seconds == 2
           && accepted.amber_seconds == 1);
    assert(mock_traffic_road1() == TRAFFIC_RED);
    assert(mock_traffic_road2() == TRAFFIC_RED);
}

int main(void)
{
    test_validation();
    test_editing();
    puts("PASS: Tuning bounds, invalid confirmation and 5800 valid timing sets");
    return 0;
}
