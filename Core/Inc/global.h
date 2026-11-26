#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdbool.h>
#include <stdint.h>

#include "traffic_light.h"

enum {
    TRAFFIC_TICKS_PER_SECOND = 100,
    FSM_ALL_RED_TICKS = 100,
    FSM_LCD_COLUMNS = 16,
    FSM_LCD_ROWS = 2
};

typedef enum {
    SYSTEM_BOOT,
    SYSTEM_AUTOMATIC,
    SYSTEM_MANUAL,
    SYSTEM_TUNING,
    SYSTEM_SWITCH_AMBER,
    SYSTEM_SWITCH_RED,
    SYSTEM_FAULT
} system_mode_t;

typedef struct {
    uint32_t red_seconds;
    uint32_t green_seconds;
    uint32_t amber_seconds;
} traffic_timing_t;

extern system_mode_t system_mode;
extern traffic_timing_t active_timing;
extern traffic_color_t current_road1;
extern traffic_color_t current_road2;

void global_reset(void);
bool traffic_timing_valid(const traffic_timing_t *timing);
uint32_t traffic_clearance_ticks(const traffic_timing_t *timing);
/* Zero duration keeps the pair indefinitely and cancels the phase timer. */
bool fsm_set_phase(traffic_color_t road1, traffic_color_t road2,
                   uint32_t duration_ticks);

#endif
