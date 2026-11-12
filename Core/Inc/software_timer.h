#ifndef SOFTWARE_TIMER_H
#define SOFTWARE_TIMER_H

#include <stdbool.h>
#include <stdint.h>

static inline uint32_t sw_timer_ms_to_ticks(uint32_t ms)
{
    return ms / 10U + (ms % 10U != 0U);
}

#define SW_TIMER_MS_TO_TICKS(ms) sw_timer_ms_to_ticks(ms)

typedef enum {
    SW_TIMER_TRAFFIC_PHASE = 0,
    SW_TIMER_COUNT
} sw_timer_id_t;

/* Call these functions from the main context. */
void sw_timer_init(void);
void sw_timer_tick(void);
bool sw_timer_start(sw_timer_id_t id, uint32_t duration_ticks);
bool sw_timer_cancel(sw_timer_id_t id);
bool sw_timer_take_timeout(sw_timer_id_t id);

#endif
