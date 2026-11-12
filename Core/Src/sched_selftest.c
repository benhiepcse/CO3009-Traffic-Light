#include "sched_selftest.h"

#ifdef SCH_SELFTEST
#include "main.h"
#include "scheduler.h"

#define TRACE_CAPACITY 32U

typedef struct {
    uint32_t elapsed_ms;
    uint32_t task_ms;
} TraceEntry;

static volatile TraceEntry trace[TRACE_CAPACITY];
static volatile uint32_t trace_count;
static volatile uint32_t count_500;
static volatile uint32_t count_1000;
static volatile uint32_t count_3000;
static volatile uint8_t one_shot_delete_result;
static uint32_t started_ms;
static uint32_t one_shot_id;

static void record(uint32_t task_ms)
{
    uint32_t index = trace_count;
    if (index < TRACE_CAPACITY) {
        trace[index].elapsed_ms = HAL_GetTick() - started_ms;
        trace[index].task_ms = task_ms;
        trace_count = index + 1U;
    }
}

static void run_500(void)
{
    count_500++;
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    record(500U);
}

static void run_1000(void)
{
    count_1000++;
    record(1000U);
}

static void run_3000(void)
{
    count_3000++;
    one_shot_delete_result = SCH_Delete_Task(one_shot_id);
    record(3000U);
}

void SCH_SelfTest_Init(void)
{
    SCH_Init();
    started_ms = HAL_GetTick();
    one_shot_delete_result = UINT8_MAX;
    uint32_t first = SCH_Add_Task(run_500, 50U, 50U);
    uint32_t second = SCH_Add_Task(run_1000, 100U, 100U);
    one_shot_id = SCH_Add_Task(run_3000, 300U, 0U);

    if (first == 0U || second == 0U || one_shot_id == 0U) {
        Error_Handler();
    }
}
#endif
