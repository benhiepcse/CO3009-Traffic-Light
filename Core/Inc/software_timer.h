#ifndef SOFTWARE_TIMER_H
#define SOFTWARE_TIMER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SW_TIMER_TRAFFIC_PHASE = 0,
    SW_TIMER_COUNT
} sw_timer_id_t;

void sw_timer_init(void);
void sw_timer_tick(void);

bool sw_timer_start(sw_timer_id_t id, uint32_t duration_ticks);
bool sw_timer_cancel(sw_timer_id_t id);
bool sw_timer_take_timeout(sw_timer_id_t id);

#endif
