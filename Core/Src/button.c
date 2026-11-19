#include "button.h"

#include <stddef.h>

#include "hardware_profile.h"

#ifndef BUTTON_HOST_TEST
#include "main.h"
#else
extern uint8_t button_test_read_level(button_id_t id);
#endif

#define BUTTON_EVENT_QUEUE_SIZE 8U

typedef struct {
    bool stable_pressed;
    bool candidate_pressed;
    uint8_t candidate_ticks;
    uint16_t held_ticks;
    bool long_press_sent;
} button_state_t;

static button_state_t states[BUTTON_COUNT];
static button_event_t event_queue[BUTTON_EVENT_QUEUE_SIZE];
static uint8_t queue_head;
static uint8_t queue_tail;
static uint8_t queue_count;

static uint8_t read_level(button_id_t id)
{
#ifdef BUTTON_HOST_TEST
    return button_test_read_level(id);
#else
    static GPIO_TypeDef *const ports[BUTTON_COUNT] = {
        BTN_D7_GPIO_Port,
        BTN_D8_GPIO_Port,
        BTN_D9_GPIO_Port,
        BTN_D10_GPIO_Port,
        BTN_USER_GPIO_Port
    };
    static const uint16_t pins[BUTTON_COUNT] = {
        BTN_D7_Pin,
        BTN_D8_Pin,
        BTN_D9_Pin,
        BTN_D10_Pin,
        BTN_USER_Pin
    };

    return (HAL_GPIO_ReadPin(ports[id], pins[id]) == GPIO_PIN_SET) ? 1U : 0U;
#endif
}

static uint8_t active_level(button_id_t id)
{
    static const uint8_t active_levels[BUTTON_COUNT] = {
        BUTTON_D7_ACTIVE_LEVEL,
        BUTTON_D8_ACTIVE_LEVEL,
        BUTTON_D9_ACTIVE_LEVEL,
        BUTTON_D10_ACTIVE_LEVEL,
        BUTTON_USER_ACTIVE_LEVEL
    };

    return active_levels[id];
}

static bool is_pressed(button_id_t id)
{
    return read_level(id) == active_level(id);
}

static void enqueue_event(button_id_t id, button_event_type_t type)
{
    if (queue_count >= BUTTON_EVENT_QUEUE_SIZE) {
        return;
    }

    event_queue[queue_tail].id = id;
    event_queue[queue_tail].type = type;
    queue_tail = (uint8_t)((queue_tail + 1U) % BUTTON_EVENT_QUEUE_SIZE);
    queue_count++;
}

void button_init(void)
{
    button_id_t id;

    for (id = BUTTON_D7; id < BUTTON_COUNT; id++) {
        states[id].stable_pressed = false;
        states[id].candidate_pressed = false;
        states[id].candidate_ticks = 0U;
        states[id].held_ticks = 0U;
        states[id].long_press_sent = false;
    }
    button_clear_events();
}

void button_poll(void)
{
    button_id_t id;

    for (id = BUTTON_D7; id < BUTTON_COUNT; id++) {
        button_state_t *state = &states[id];
        bool pressed = is_pressed(id);

        if (pressed != state->candidate_pressed) {
            state->candidate_pressed = pressed;
            state->candidate_ticks = 1U;
        } else if (state->candidate_ticks < BUTTON_DEBOUNCE_TICKS) {
            state->candidate_ticks++;
        }

        if ((state->candidate_ticks >= BUTTON_DEBOUNCE_TICKS) &&
            (state->stable_pressed != state->candidate_pressed)) {
            state->stable_pressed = state->candidate_pressed;
            state->held_ticks = 0U;
            state->long_press_sent = false;

            if (state->stable_pressed) {
                enqueue_event(id, BUTTON_EVENT_PRESS);
            }
        } else if (state->stable_pressed && pressed && !state->long_press_sent) {
            state->held_ticks++;
            if (state->held_ticks >= BUTTON_LONG_PRESS_TICKS) {
                enqueue_event(id, BUTTON_EVENT_LONG_PRESS);
                state->long_press_sent = true;
            }
        }
    }
}

bool button_take_event(button_event_t *out)
{
    if ((out == NULL) || (queue_count == 0U)) {
        return false;
    }

    *out = event_queue[queue_head];
    queue_head = (uint8_t)((queue_head + 1U) % BUTTON_EVENT_QUEUE_SIZE);
    queue_count--;
    return true;
}

void button_clear_events(void)
{
    queue_head = 0U;
    queue_tail = 0U;
    queue_count = 0U;
}
