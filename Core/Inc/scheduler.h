#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

void SCH_Init(void);
uint32_t SCH_Add_Task(void (*task)(void), uint32_t delay_ticks,
                      uint32_t period_ticks);
uint8_t SCH_Delete_Task(uint32_t task_id);
void SCH_Update(void);
void SCH_Dispatch_Tasks(void);

#endif
