#include <assert.h>
#include <stdio.h>

#include "scheduler.h"
#include "scheduler_test_hooks.h"

static void noop(void)
{
}

static void update_ticks(unsigned count)
{
    for (unsigned tick = 0; tick < count; tick++) {
        SCH_Update();
    }
}

static void expect_queue(const uint32_t *ids, const uint32_t *delays,
                         size_t expected_count)
{
    SchedulerTestEntry entries[17];
    size_t count = SCH_Test_Snapshot(entries, 17U);

    assert(count == expected_count);
    for (size_t i = 0; i < count; i++) {
        assert(entries[i].id == ids[i]);
        assert(entries[i].delay_ticks == delays[i]);
    }
}

static void test_insert_and_delete(void)
{
    SCH_Init();

    uint32_t b = SCH_Add_Task(noop, 12U, 0U);
    uint32_t a = SCH_Add_Task(noop, 5U, 0U);
    uint32_t c = SCH_Add_Task(noop, 12U, 0U);
    uint32_t d = SCH_Add_Task(noop, 9U, 0U);
    uint32_t e = SCH_Add_Task(noop, 15U, 0U);

    assert(a && b && c && d && e);
    expect_queue((uint32_t[]){a, d, b, c, e},
                 (uint32_t[]){5U, 4U, 3U, 0U, 3U}, 5U);

    assert(SCH_Delete_Task(d) == 1U);
    expect_queue((uint32_t[]){a, b, c, e},
                 (uint32_t[]){5U, 7U, 0U, 3U}, 4U);

    assert(SCH_Delete_Task(a) == 1U);
    assert(SCH_Delete_Task(e) == 1U);
    expect_queue((uint32_t[]){b, c}, (uint32_t[]){12U, 0U}, 2U);

    uint32_t immediate = SCH_Add_Task(noop, 0U, 0U);
    assert(immediate != 0U);
    expect_queue((uint32_t[]){immediate, b, c},
                 (uint32_t[]){0U, 12U, 0U}, 3U);
}

static void test_capacity_and_invalid_ids(void)
{
    SCH_Init();
    assert(SCH_Add_Task(NULL, 1U, 0U) == 0U);
    assert(SCH_Delete_Task(0U) == 0U);
    assert(SCH_Delete_Task(1000U) == 0U);

    uint32_t ids[16];
    for (size_t i = 0; i < 16U; i++) {
        ids[i] = SCH_Add_Task(noop, 1U, 0U);
        assert(ids[i] != 0U);
    }
    assert(SCH_Add_Task(noop, 1U, 0U) == 0U);
    assert(SCH_Delete_Task(ids[5]) == 1U);

    uint32_t replacement = SCH_Add_Task(noop, 1U, 0U);
    assert(replacement != 0U && replacement != ids[5]);
    assert(SCH_Delete_Task(ids[5]) == 0U);
    assert(SCH_Delete_Task(replacement) == 1U);
}

static void test_id_wrap(void)
{
    SCH_Init();
    assert(SCH_Add_Task(noop, 1U, 0U) == 1U);

    SCH_Test_SetNextId(UINT32_MAX);
    assert(SCH_Add_Task(noop, 1U, 0U) == UINT32_MAX);
    assert(SCH_Add_Task(noop, 1U, 0U) == 2U);
}

static void test_tick_and_pending_ticks(void)
{
    SCH_Init();
    SCH_Update();
    assert(SCH_Test_PendingTicks() == 0U);

    uint32_t a = SCH_Add_Task(noop, 3U, 0U);
    uint32_t b = SCH_Add_Task(noop, 5U, 0U);
    uint32_t c = SCH_Add_Task(noop, 9U, 0U);

    update_ticks(2U);
    expect_queue((uint32_t[]){a, b, c}, (uint32_t[]){1U, 2U, 4U}, 3U);

    SCH_Update();
    expect_queue((uint32_t[]){a, b, c}, (uint32_t[]){0U, 2U, 4U}, 3U);
    assert(SCH_Test_PendingTicks() == 0U);

    update_ticks(3U);
    assert(SCH_Test_PendingTicks() == 3U);

    uint32_t d = SCH_Add_Task(noop, 0U, 0U);
    assert(d != 0U);
    assert(SCH_Test_PendingTicks() == 0U);
    expect_queue((uint32_t[]){a, b, d, c},
                 (uint32_t[]){0U, 0U, 0U, 3U}, 4U);

    SCH_Update();
    assert(SCH_Delete_Task(a) == 1U);
    expect_queue((uint32_t[]){b, d, c}, (uint32_t[]){0U, 0U, 2U}, 3U);

    update_ticks(10U);
    assert(SCH_Delete_Task(b) == 1U);
    expect_queue((uint32_t[]){d, c}, (uint32_t[]){0U, 0U}, 2U);
    assert(SCH_Delete_Task(d) == 1U);
    assert(SCH_Delete_Task(c) == 1U);

    SCH_Update();
    assert(SCH_Test_PendingTicks() == 0U);
    uint32_t fresh = SCH_Add_Task(noop, 2U, 0U);
    assert(fresh != 0U);
    expect_queue((uint32_t[]){fresh}, (uint32_t[]){2U}, 1U);
}

static void test_max_delay(void)
{
    SCH_Init();

    uint32_t first = SCH_Add_Task(noop, 1U, 0U);
    uint32_t last = SCH_Add_Task(noop, UINT32_MAX, 0U);

    assert(first != 0U && last != 0U);
    assert(SCH_Delete_Task(first) == 1U);
    expect_queue((uint32_t[]){last}, (uint32_t[]){UINT32_MAX}, 1U);
}

int main(void)
{
    test_insert_and_delete();
    puts("PASS: insert/delete, FIFO ties, delay zero");

    test_capacity_and_invalid_ids();
    puts("PASS: capacity, invalid IDs, slot reuse");

    test_id_wrap();
    puts("PASS: ID wrap skips zero and live IDs");

    test_tick_and_pending_ticks();
    puts("PASS: exact due tick and pending tick normalization");

    test_max_delay();
    puts("PASS: maximum delay survives head deletion");

    puts("CO3009-07 scheduler test: PASS");
    return 0;
}
