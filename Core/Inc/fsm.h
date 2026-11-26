#ifndef FSM_H
#define FSM_H

void fsm_init(void);
void fsm_run(void);
/* Refresh at most one changed LCD line, from the main context. */
void fsm_display_run(void);

#endif
