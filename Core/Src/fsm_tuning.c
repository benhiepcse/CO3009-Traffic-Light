#include "fsm_tuning.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    TUNING_RED = 0,
    TUNING_GREEN,
    TUNING_AMBER,
    TUNING_FIELD_COUNT
} tuning_field_t;

typedef struct {
    const char *title;
    uint32_t *seconds;
    uint32_t min_seconds;
    uint32_t max_seconds;
} tuning_field_config_t;

static traffic_timing_t draft_timing;
static tuning_field_t selected_field;
static bool show_timing_error;

static const tuning_field_config_t fields[TUNING_FIELD_COUNT] = {
    [TUNING_RED] = {
        "TUNE RED",
        &draft_timing.red_seconds,
        TRAFFIC_RED_MIN_SECONDS,
        TRAFFIC_RED_MAX_SECONDS
    },
    [TUNING_GREEN] = {
        "TUNE GREEN",
        &draft_timing.green_seconds,
        TRAFFIC_GREEN_MIN_SECONDS,
        TRAFFIC_GREEN_MAX_SECONDS
    },
    [TUNING_AMBER] = {
        "TUNE AMBER",
        &draft_timing.amber_seconds,
        TRAFFIC_AMBER_MIN_SECONDS,
        TRAFFIC_AMBER_MAX_SECONDS
    }
};

void fsm_tuning_enter(void)
{
    draft_timing = active_timing;
    selected_field = TUNING_RED;
    show_timing_error = false;

    /* Keep both roads red and cancel the transition timer. */
    (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, 0U);
}

bool fsm_tuning_run(button_id_t button, traffic_timing_t *accepted_timing)
{
    const tuning_field_config_t *field = &fields[selected_field];

    switch (button) {
    case BUTTON_D8:
        selected_field = (tuning_field_t)(
            (selected_field + 1U) % TUNING_FIELD_COUNT
        );
        show_timing_error = false;
        break;

    case BUTTON_D9:
        if (*field->seconds < field->max_seconds) {
            ++(*field->seconds);
        }
        show_timing_error = false;
        break;

    case BUTTON_D10:
        if (*field->seconds > field->min_seconds) {
            --(*field->seconds);
        }
        show_timing_error = false;
        break;

    case BUTTON_D7:
        show_timing_error = !traffic_timing_valid(&draft_timing);

        if (!show_timing_error && accepted_timing != NULL) {
            *accepted_timing = draft_timing;
            return true;
        }
        break;

    default:
        break;
    }

    return false;
}

void fsm_tuning_format(char lines[FSM_LCD_ROWS][FSM_LCD_COLUMNS + 1])
{
    const tuning_field_config_t *field = &fields[selected_field];

    (void)snprintf(lines[0], sizeof lines[0], "%s", field->title);

    if (show_timing_error) {
        (void)snprintf(lines[1], sizeof lines[1], "R < G+Y+2");
    } else {
        (void)snprintf(lines[1], sizeof lines[1], "%lu s",
                       (unsigned long)*field->seconds);
    }
}
