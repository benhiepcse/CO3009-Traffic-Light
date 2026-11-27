#ifndef FSM_AUTOMATIC_H
#define FSM_AUTOMATIC_H

#include <stdint.h>

/* Enter after the caller has completed its all-red interval. */
void fsm_automatic_enter(void);
void fsm_automatic_run(void);
uint8_t fsm_automatic_road1_seconds_left(void);
uint8_t fsm_automatic_road2_seconds_left(void);

#endif
