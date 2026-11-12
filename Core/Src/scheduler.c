#include "scheduler.h"

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

    /* Walk past equal deadlines to preserve insertion order. */
    int *link = &head;
    while (*link != SCH_NO_SLOT && tasks[*link].delay_ticks <= delay_ticks) {
        delay_ticks -= tasks[*link].delay_ticks;
        link = &tasks[*link].next;
    }

    uint32_t id = allocate_id();
    int successor = *link;

    tasks[free_slot] = (SchedulerTask){
        .task = task,
        .id = id,
        .delay_ticks = delay_ticks,
        .period_ticks = period_ticks,
        .next = successor
    };

    if (successor != SCH_NO_SLOT) {
        tasks[successor].delay_ticks -= delay_ticks;
    }

    *link = free_slot;
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
    /* TODO-08: Run due tasks and reschedule periodic tasks. */
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
