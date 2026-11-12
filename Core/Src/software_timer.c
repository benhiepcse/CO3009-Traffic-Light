#include "software_timer.h"

typedef struct {
    uint32_t remaining_ticks;
    bool running;
    bool timeout;
} SoftwareTimer;

static SoftwareTimer timers[SW_TIMER_COUNT];

static bool valid_id(sw_timer_id_t id)
{
    return (uint32_t)id < (uint32_t)SW_TIMER_COUNT;
}

void sw_timer_init(void)
{
    for (uint32_t i = 0U; i < SW_TIMER_COUNT; i++) {
        timers[i] = (SoftwareTimer){0};
    }
}

bool sw_timer_start(sw_timer_id_t id, uint32_t duration_ticks)
{
    if (!valid_id(id) || duration_ticks == 0U) {
        return false;
    }

    /* Expiry occurs on the duration_ticks-th call to sw_timer_tick.
       The elapsed wall time depends on the scheduler cadence. */
    timers[id] = (SoftwareTimer){
        .remaining_ticks = duration_ticks,
        .running = true
    };
    return true;
}

void sw_timer_tick(void)
{
    for (uint32_t i = 0U; i < SW_TIMER_COUNT; i++) {
        SoftwareTimer *timer = &timers[i];
        if (timer->running && --timer->remaining_ticks == 0U) {
            timer->running = false;
            timer->timeout = true;
        }
    }
}

bool sw_timer_cancel(sw_timer_id_t id)
{
    if (!valid_id(id)) {
        return false;
    }

    timers[id] = (SoftwareTimer){0};
    return true;
}

bool sw_timer_take_timeout(sw_timer_id_t id)
{
    if (!valid_id(id) || !timers[id].timeout) {
        return false;
    }

    timers[id].timeout = false;
    return true;
}
