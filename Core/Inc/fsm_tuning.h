#ifndef FSM_TUNING_H
#define FSM_TUNING_H

#include <stdbool.h>

#include "button.h"
#include "global.h"

void fsm_tuning_enter(void);

/* Returns true when D7 confirms a valid timing configuration. */
bool fsm_tuning_run(button_id_t button, traffic_timing_t *accepted_timing);

void fsm_tuning_format(char lines[FSM_LCD_ROWS][FSM_LCD_COLUMNS + 1]);

#endif
