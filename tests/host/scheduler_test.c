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

static unsigned now;
static unsigned count_50;
static unsigned count_100;
static unsigned count_300;
static unsigned count_fast;
static uint32_t shot_id;

static void periodic_50(void)
{
    assert(now == 50U * ++count_50);
}

static void periodic_100(void)
{
    assert(now == 100U * ++count_100);
}

static void shot_300(void)
{
    assert(now == 300U && ++count_300 == 1U);
    assert(SCH_Delete_Task(shot_id) == 0U);
}

static void fast(void)
{
    assert(now == ++count_fast);
}

static void test_dispatch_schedule(void)
{
    SCH_Init();
    assert(SCH_Add_Task(periodic_50, 50U, 50U));
    assert(SCH_Add_Task(periodic_100, 100U, 100U));
    shot_id = SCH_Add_Task(shot_300, 300U, 0U);
    assert(shot_id);
    assert(SCH_Add_Task(fast, 1U, 1U));

    for (now = 1U; now <= 10000U; now++) {
        SCH_Update();
        SCH_Dispatch_Tasks();
    }
    assert(count_50 == 200U && count_100 == 100U);
    assert(count_300 == 1U && count_fast == 10000U);
}

static unsigned start_count;

static void start_now(void) { start_count++; }

static void test_immediate_and_external_delete(void)
{
    SCH_Init();
    uint32_t id = SCH_Add_Task(start_now, 0U, 2U);
    assert(id && SCH_Add_Task(start_now, 3U, 0U));
    SCH_Dispatch_Tasks();
    assert(start_count == 1U);
    SCH_Update();
    assert(SCH_Delete_Task(id) == 1U);
    SCH_Update();
    SCH_Dispatch_Tasks();
    assert(start_count == 1U);
    SCH_Update();
    SCH_Dispatch_Tasks();
    assert(start_count == 2U);
}

static unsigned order[4];
static unsigned used;

static void record_a(void) { order[used++] = 1U; }
static void record_b(void) { order[used++] = 2U; }
static void record_c(void) { order[used++] = 3U; }

static void test_late_dispatch(void)
{
    SCH_Init();
    uint32_t a = SCH_Add_Task(record_a, 3U, 4U);
    assert(SCH_Add_Task(record_b, 5U, 0U));
    uint32_t c = SCH_Add_Task(record_c, 9U, 0U);
    assert(a && c);
    update_ticks(6U);
    SCH_Dispatch_Tasks();
    assert(used == 2U && order[0] == 1U && order[1] == 2U);
    expect_queue((uint32_t[]){c, a}, (uint32_t[]){3U, 1U}, 2U);
    update_ticks(3U);
    SCH_Dispatch_Tasks();
    assert(used == 3U && order[2] == 3U);
    SCH_Update();
    SCH_Dispatch_Tasks();
    assert(used == 4U && order[3] == 1U);
}

static uint32_t self_id;
static uint32_t victim_id;
static unsigned spawned;

static void added_callback(void) { spawned++; }

static void edit_queue(void)
{
    assert(SCH_Delete_Task(self_id) == 1U);
    assert(SCH_Delete_Task(victim_id) == 1U);
    assert(SCH_Add_Task(added_callback, 1U, 0U));
    SCH_Update(); /* Simulate a timer IRQ during the callback. */
    SCH_Dispatch_Tasks();
    assert(spawned == 0U);
}

static void test_callback_mutations(void)
{
    SCH_Init();
    self_id = SCH_Add_Task(edit_queue, 0U, 1U);
    victim_id = SCH_Add_Task(noop, 0U, 0U);
    assert(self_id && victim_id);
    SCH_Dispatch_Tasks();
    assert(spawned == 1U);
    expect_queue(NULL, NULL, 0U);
}

static unsigned chain_count;

static void chain(void)
{
    chain_count++;
    if (chain_count < 17U) {
        assert(SCH_Add_Task(chain, 0U, 0U));
    }
}

static void test_dispatch_limit(void)
{
    SCH_Init();
    assert(SCH_Add_Task(chain, 0U, 0U));
    SCH_Dispatch_Tasks();
    assert(chain_count == 16U);
    SCH_Dispatch_Tasks();
    assert(chain_count == 17U);
    expect_queue(NULL, NULL, 0U);
}

static unsigned ties;
static void tie_first(void) { assert(++ties == 1U); }
static void tie_second(void) { assert(++ties == 2U); }

static void test_dispatch_ties_and_reuse(void)
{
    SCH_Init();
    uint32_t first = SCH_Add_Task(tie_first, 1U, 0U);
    assert(first && SCH_Add_Task(tie_second, 1U, 0U));
    for (unsigned i = 0; i < 14U; i++) {
        assert(SCH_Add_Task(noop, 5U, 0U));
    }
    assert(SCH_Add_Task(noop, 5U, 0U) == 0U);
    SCH_Update();
    SCH_Dispatch_Tasks();
    assert(ties == 2U && SCH_Delete_Task(first) == 0U);
    assert(SCH_Add_Task(noop, 5U, 0U));
    assert(SCH_Add_Task(noop, 5U, 0U));
    assert(SCH_Add_Task(noop, 5U, 0U) == 0U);
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
    test_dispatch_schedule();
    puts("PASS: 10000 ticks, 50/100/1 tick periodic and 300 tick one-shot");
    test_immediate_and_external_delete();
    puts("PASS: immediate periodic dispatch and external deletion");
    test_late_dispatch();
    puts("PASS: late dispatch preserves queued deadlines");
    test_callback_mutations();
    puts("PASS: callback add/delete, simulated IRQ, recursion guard");
    test_dispatch_limit();
    puts("PASS: dispatch budget and continuation");
    test_dispatch_ties_and_reuse();
    puts("PASS: FIFO callbacks and one-shot slot reuse");
    puts("CO3009-08 scheduler test: PASS");
    return 0;
}
