#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

typedef enum {
    BUTTON_D7 = 0,
    BUTTON_D8,
    BUTTON_D9,
    BUTTON_D10,
    BUTTON_USER,
    BUTTON_COUNT
} button_id_t;

typedef enum {
    BUTTON_EVENT_PRESS = 0,
    BUTTON_EVENT_LONG_PRESS
} button_event_type_t;

typedef struct {
    button_id_t id;
    button_event_type_t type;
} button_event_t;

void button_init(void);
void button_poll(void);
bool button_take_event(button_event_t *out);
void button_clear_events(void);

#endif
