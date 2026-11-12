#include <assert.h>
#include <stdio.h>

#include "scheduler.h"
#include "software_timer.h"

#define PHASE SW_TIMER_TRAFFIC_PHASE

static void test_conversion(void)
{
    assert(SW_TIMER_MS_TO_TICKS(0U) == 0U);
    assert(SW_TIMER_MS_TO_TICKS(1U) == 1U);
    assert(SW_TIMER_MS_TO_TICKS(10U) == 1U);
    assert(SW_TIMER_MS_TO_TICKS(25U) == 3U);
    assert(SW_TIMER_MS_TO_TICKS(UINT32_MAX) == 429496730U);

    uint32_t ms = 25U;
    assert(SW_TIMER_MS_TO_TICKS(ms++) == 3U);
    assert(ms == 26U);
}

static void test_exact_expiry(void)
{
    sw_timer_init();
    sw_timer_tick();
    assert(!sw_timer_take_timeout(PHASE));

    assert(sw_timer_start(PHASE, 3U));
    sw_timer_tick();
    sw_timer_tick();
    assert(!sw_timer_take_timeout(PHASE));
    sw_timer_tick();
    assert(sw_timer_take_timeout(PHASE));
    assert(!sw_timer_take_timeout(PHASE));
    sw_timer_tick();
    assert(!sw_timer_take_timeout(PHASE));

    assert(sw_timer_start(PHASE, 1U));
    assert(!sw_timer_take_timeout(PHASE));
    sw_timer_tick();
    assert(sw_timer_take_timeout(PHASE));
}

static void test_restart_cancel_and_init(void)
{
    sw_timer_init();
    assert(sw_timer_start(PHASE, 3U));
    sw_timer_tick();
    assert(sw_timer_start(PHASE, 2U));
    sw_timer_tick();
    assert(!sw_timer_take_timeout(PHASE));
    sw_timer_tick();

    /* A new start clears the unread timeout from the previous run. */
    assert(sw_timer_start(PHASE, 1U));
    assert(!sw_timer_take_timeout(PHASE));
    sw_timer_tick();
    assert(sw_timer_cancel(PHASE));
    assert(!sw_timer_take_timeout(PHASE));

    assert(sw_timer_start(PHASE, 1U));
    assert(sw_timer_cancel(PHASE));
    sw_timer_tick();
    sw_timer_tick();
    assert(!sw_timer_take_timeout(PHASE));

    assert(sw_timer_start(PHASE, 1U));
    sw_timer_tick();
    sw_timer_init();
    assert(!sw_timer_take_timeout(PHASE));

    assert(sw_timer_start(PHASE, 1U));
    sw_timer_init();
    sw_timer_tick();
    assert(!sw_timer_take_timeout(PHASE));
}

static void test_invalid_input(void)
{
    const sw_timer_id_t bad_ids[] = {
        SW_TIMER_COUNT,
        (sw_timer_id_t)UINT32_MAX
    };

    sw_timer_init();
    assert(sw_timer_start(PHASE, 2U));
    sw_timer_tick();

    for (unsigned i = 0U; i < sizeof bad_ids / sizeof bad_ids[0]; i++) {
        assert(!sw_timer_start(bad_ids[i], 1U));
        assert(!sw_timer_cancel(bad_ids[i]));
        assert(!sw_timer_take_timeout(bad_ids[i]));
    }

    assert(!sw_timer_start(PHASE, 0U));
    assert(!sw_timer_take_timeout(PHASE));
    sw_timer_tick();
    assert(sw_timer_take_timeout(PHASE));
}

static void test_max_duration(void)
{
    sw_timer_init();
    assert(sw_timer_start(PHASE, UINT32_MAX));

    for (unsigned i = 0U; i < 4U; i++) {
        sw_timer_tick();
        assert(!sw_timer_take_timeout(PHASE));
    }

    assert(sw_timer_cancel(PHASE));
    sw_timer_tick();
    assert(!sw_timer_take_timeout(PHASE));

    assert(sw_timer_start(PHASE, UINT32_MAX));
    sw_timer_tick();
    assert(sw_timer_start(PHASE, 1U));
    sw_timer_tick();
    assert(sw_timer_take_timeout(PHASE));
}

static void test_scheduler_integration(void)
{
    SCH_Init();
    sw_timer_init();

    assert(SCH_Add_Task(sw_timer_tick, 1U, 1U) != 0U);
    assert(sw_timer_start(PHASE, SW_TIMER_MS_TO_TICKS(25U)));

    SCH_Dispatch_Tasks();
    assert(!sw_timer_take_timeout(PHASE));

    for (unsigned tick = 1U; tick <= 4U; tick++) {
        SCH_Update();
        assert(!sw_timer_take_timeout(PHASE));

        SCH_Dispatch_Tasks();
        assert(sw_timer_take_timeout(PHASE) == (tick == 3U));
    }
}

int main(void)
{
    test_conversion();
    puts("PASS: ms to ticks and single argument evaluation");

    test_exact_expiry();
    puts("PASS: exact expiry and one-time timeout");

    test_restart_cancel_and_init();
    puts("PASS: restart, cancel and init clear old state");

    test_invalid_input();
    puts("PASS: invalid ID and zero duration preserve timer");

    test_max_duration();
    puts("PASS: UINT32_MAX duration, early ticks and reset");

    test_scheduler_integration();
    puts("PASS: real scheduler dispatches software timer");

    puts("CO3009-09 software timer test: PASS");
    return 0;
}
