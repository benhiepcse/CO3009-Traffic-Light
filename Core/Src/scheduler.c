#include "scheduler.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef SCH_HOST_TEST
#include "../../tests/host/scheduler_test_hooks.h"
#else
#include "stm32f1xx.h"
#endif

#define SCH_MAX_TASKS 16
#define SCH_NO_SLOT (-1)

typedef struct {
    void (*task)(void);
    uint32_t id;
    uint32_t delay_ticks;
    uint32_t period_ticks;
    int next;
} SchedulerTask;

static SchedulerTask tasks[SCH_MAX_TASKS];
static int head = SCH_NO_SLOT;
static uint32_t next_id = 1U;
static uint32_t pending_ticks;
static bool dispatching;

static uint32_t lock_scheduler(void)
{
#ifdef SCH_HOST_TEST
    return 0U;
#else
    uint32_t saved_primask = __get_PRIMASK();

    __disable_irq();
    return saved_primask;
#endif
}

static void unlock_scheduler(uint32_t saved_primask)
{
#ifdef SCH_HOST_TEST
    (void)saved_primask;
#else
    __set_PRIMASK(saved_primask);
#endif
}

/* Apply ticks that elapsed while a due task remained at the head. */
static void normalize_pending_ticks(void)
{
    uint32_t elapsed = pending_ticks;

    pending_ticks = 0U;
    for (int slot = head; slot != SCH_NO_SLOT && elapsed != 0U;
         slot = tasks[slot].next) {
        if (tasks[slot].delay_ticks > elapsed) {
            tasks[slot].delay_ticks -= elapsed;
            break;
        }

        elapsed -= tasks[slot].delay_ticks;
        tasks[slot].delay_ticks = 0U;
    }
}

static int find_task(uint32_t id)
{
    for (int slot = head; slot != SCH_NO_SLOT; slot = tasks[slot].next) {
        if (tasks[slot].id == id) {
            return slot;
        }
    }

    return SCH_NO_SLOT;
}

static uint32_t allocate_id(void)
{
    uint32_t id;

    do {
        id = next_id;
        next_id++;
        if (next_id == 0U) {
            next_id = 1U;
        }
    } while (find_task(id) != SCH_NO_SLOT);

    return id;
}

/* Caller holds the lock and has normalized the queue. */
static void insert_task(int slot, uint32_t delay_ticks)
{
    int *link = &head;
    while (*link != SCH_NO_SLOT && tasks[*link].delay_ticks <= delay_ticks) {
        delay_ticks -= tasks[*link].delay_ticks;
        link = &tasks[*link].next;
    }

    int successor = *link;
    tasks[slot].delay_ticks = delay_ticks;
    tasks[slot].next = successor;
    if (successor != SCH_NO_SLOT) {
        tasks[successor].delay_ticks -= delay_ticks;
    }
    *link = slot;
}

void SCH_Init(void)
{
    uint32_t saved_primask = lock_scheduler();

    for (int slot = 0; slot < SCH_MAX_TASKS; slot++) {
        tasks[slot] = (SchedulerTask){0};
    }

    head = SCH_NO_SLOT;
    next_id = 1U;
    pending_ticks = 0U;
    unlock_scheduler(saved_primask);
}

uint32_t SCH_Add_Task(void (*task)(void),
                      uint32_t delay_ticks,
                      uint32_t period_ticks)
{
    if (task == NULL) {
        return 0U;
    }

    uint32_t saved_primask = lock_scheduler();
    normalize_pending_ticks();

    int free_slot = 0;
    while (free_slot < SCH_MAX_TASKS && tasks[free_slot].task != NULL) {
        free_slot++;
    }

    if (free_slot == SCH_MAX_TASKS) {
        unlock_scheduler(saved_primask);
        return 0U;
    }

    uint32_t id = allocate_id();
    tasks[free_slot] = (SchedulerTask){
        .task = task,
        .id = id,
        .period_ticks = period_ticks
    };
    insert_task(free_slot, delay_ticks);
    unlock_scheduler(saved_primask);
    return id;
}

uint8_t SCH_Delete_Task(uint32_t task_id)
{
    if (task_id == 0U) {
        return 0U;
    }

    uint32_t saved_primask = lock_scheduler();
    normalize_pending_ticks();

    int *link = &head;
    while (*link != SCH_NO_SLOT && tasks[*link].id != task_id) {
        link = &tasks[*link].next;
    }

    int slot = *link;
    if (slot == SCH_NO_SLOT) {
        unlock_scheduler(saved_primask);
        return 0U;
    }

    int successor = tasks[slot].next;
    if (successor != SCH_NO_SLOT) {
        tasks[successor].delay_ticks += tasks[slot].delay_ticks;
    }

    *link = successor;
    tasks[slot] = (SchedulerTask){0};
    unlock_scheduler(saved_primask);
    return 1U;
}

void SCH_Update(void)
{
    if (head == SCH_NO_SLOT) {
        return;
    }

    if (tasks[head].delay_ticks > 0U) {
        tasks[head].delay_ticks--;
    } else if (pending_ticks < UINT32_MAX) {
        pending_ticks++;
    }
}

void SCH_Dispatch_Tasks(void)
{
    if (dispatching) {
        return;
    }

    dispatching = true;
    for (int run = 0; run < SCH_MAX_TASKS; run++) {
        uint32_t saved_primask = lock_scheduler();
        normalize_pending_ticks();

        int slot = head;
        if (slot == SCH_NO_SLOT || tasks[slot].delay_ticks != 0U) {
            unlock_scheduler(saved_primask);
            break;
        }

        void (*callback)(void) = tasks[slot].task;
        uint32_t period = tasks[slot].period_ticks;
        head = tasks[slot].next;

        if (period == 0U) {
            /* The one-shot ID is already invalid when its callback runs. */
            tasks[slot] = (SchedulerTask){0};
        } else {
            /* Keep the slot and ID so the callback can delete itself. */
            insert_task(slot, period);
        }

        unlock_scheduler(saved_primask);
        callback();
    }
    dispatching = false;
}

#ifdef SCH_HOST_TEST
size_t SCH_Test_Snapshot(SchedulerTestEntry *entries, size_t capacity)
{
    size_t count = 0U;

    for (int slot = head; slot != SCH_NO_SLOT && count < capacity;
         slot = tasks[slot].next) {
        entries[count].id = tasks[slot].id;
        entries[count].delay_ticks = tasks[slot].delay_ticks;
        count++;
    }

    return count;
}

uint32_t SCH_Test_PendingTicks(void)
{
    return pending_ticks;
}

void SCH_Test_SetNextId(uint32_t id)
{
    next_id = id == 0U ? 1U : id;
}
#endif
