#include "fsm_tuning.h"

#include <stddef.h>
#include <stdio.h>

typedef enum { EDIT_RED, EDIT_GREEN, EDIT_AMBER, EDIT_COUNT } tuning_field_t;

static const struct {
    const char *name;
    uint32_t minimum;
    uint32_t maximum;
} fields[EDIT_COUNT] = {
    [EDIT_RED] = { "RED", 5, 60 },
    [EDIT_GREEN] = { "GREEN", 2, 30 },
    [EDIT_AMBER] = { "AMBER", 1, 5 }
};

static traffic_timing_t draft;
static tuning_field_t selected;
static bool invalid_input;

static uint32_t *selected_value(void)
{
    switch (selected) {
    case EDIT_RED:
        return &draft.red_seconds;
    case EDIT_GREEN:
        return &draft.green_seconds;
    default:
        return &draft.amber_seconds;
    }
}

void fsm_tuning_enter(void)
{
    draft = active_timing;
    selected = EDIT_RED;
    invalid_input = false;
    (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, 0);
}

bool fsm_tuning_run(button_id_t button, traffic_timing_t *accepted)
{
    uint32_t *value = selected_value();

    switch (button) {
    case BUTTON_D7:
        invalid_input = !traffic_timing_valid(&draft);
        if (!invalid_input && accepted != NULL) {
            *accepted = draft;
            return true;
        }
        break;
    case BUTTON_D8:
        selected = (tuning_field_t)((selected + 1) % EDIT_COUNT);
        invalid_input = false;
        break;
    case BUTTON_D9:
        if (*value < fields[selected].maximum) {
            ++*value;
        }
        invalid_input = false;
        break;
    case BUTTON_D10:
        if (*value > fields[selected].minimum) {
            --*value;
        }
        invalid_input = false;
        break;
    default:
        break;
    }
    return false;
}

void fsm_tuning_format(char lines[FSM_LCD_ROWS][FSM_LCD_COLUMNS + 1])
{
    if (invalid_input) {
        (void)snprintf(lines[0], FSM_LCD_COLUMNS + 1, "%s: R < G+Y+2", fields[selected].name);
    } else {
        (void)snprintf(lines[0], FSM_LCD_COLUMNS + 1, "TUNE %s %lu s",
                       fields[selected].name, (unsigned long)*selected_value());
    }
    (void)snprintf(lines[1], FSM_LCD_COLUMNS + 1, "L1 RED %lus",
                   (unsigned long)(traffic_road1_elapsed_ms() / 1000U));
}
