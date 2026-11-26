#ifndef FSM_TUNING_H
#define FSM_TUNING_H

#include "button.h"
#include "global.h"

void fsm_tuning_enter(void);
/* Return a validated copy only when MODE confirms the draft. */
bool fsm_tuning_run(button_id_t button, traffic_timing_t *accepted);
void fsm_tuning_format(char lines[FSM_LCD_ROWS][FSM_LCD_COLUMNS + 1]);

#endif
