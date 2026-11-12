#ifndef SCHEDULER_TEST_HOOKS_H
#define SCHEDULER_TEST_HOOKS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t id;
    uint32_t delay_ticks;
} SchedulerTestEntry;

size_t SCH_Test_Snapshot(SchedulerTestEntry *entries, size_t capacity);
uint32_t SCH_Test_PendingTicks(void);
void SCH_Test_SetNextId(uint32_t id);

#endif
