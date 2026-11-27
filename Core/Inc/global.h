#ifndef GLOBAL_H
#define GLOBAL_H

#include <stdbool.h>
#include <stdint.h>

#include "traffic_light.h"

#define TRAFFIC_TICKS_PER_SECOND 100U
#define FSM_ALL_RED_TICKS        TRAFFIC_TICKS_PER_SECOND

#define FSM_LCD_ROWS             2U
#define FSM_LCD_COLUMNS          16U

#define TRAFFIC_RED_MIN_SECONDS    5U
#define TRAFFIC_RED_MAX_SECONDS   60U
#define TRAFFIC_GREEN_MIN_SECONDS  2U
#define TRAFFIC_GREEN_MAX_SECONDS 30U
#define TRAFFIC_AMBER_MIN_SECONDS  1U
#define TRAFFIC_AMBER_MAX_SECONDS  5U

typedef enum {
    SYSTEM_BOOT = 0,
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

/* Returns the duration of one all-red interval, or zero for invalid timing. */
uint32_t traffic_clearance_ticks(const traffic_timing_t *timing);

/* A duration of zero cancels the phase timer. */
bool fsm_set_phase(traffic_color_t road1,
                   traffic_color_t road2,
                   uint32_t duration_ticks);

#endif
