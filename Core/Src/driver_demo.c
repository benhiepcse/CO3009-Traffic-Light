#include "driver_demo.h"

#if defined(CO3009_TRAFFIC_DEMO) || defined(CO3009_BUTTON_DEMO)

#include <string.h>

static UART_HandleTypeDef *demo_uart;
static uint32_t last_tick_ms;

static void demo_print(const char *text)
{
    HAL_UART_Transmit(demo_uart, (uint8_t *)text, (uint16_t)strlen(text), 10U);
}

#ifdef CO3009_TRAFFIC_DEMO

#include "traffic_light.h"

#define TRAFFIC_STEP_MS 2000U

typedef struct {
    traffic_color_t road1;
    traffic_color_t road2;
    const char *label;
    bool accepted;
} traffic_step_t;

static const traffic_step_t traffic_steps[] = {
    { TRAFFIC_OFF,   TRAFFIC_RED,   "01 OFF / RED",     true },
    { TRAFFIC_AMBER, TRAFFIC_RED,   "02 AMBER / RED",   true },
    { TRAFFIC_GREEN, TRAFFIC_RED,   "03 GREEN / RED",   true },
    { TRAFFIC_RED,   TRAFFIC_GREEN, "04 RED / GREEN",   true },
    { TRAFFIC_GREEN, TRAFFIC_RED,   "05 GREEN / RED",   true },
    { TRAFFIC_RED,   TRAFFIC_RED,   "06 RED / RED",     true },
    { TRAFFIC_RED,   TRAFFIC_OFF,   "07 RED / OFF",     true },
    { TRAFFIC_RED,   TRAFFIC_AMBER, "08 RED / AMBER",   true },
    { TRAFFIC_RED,   TRAFFIC_GREEN, "09 RED / GREEN",   true },
    { TRAFFIC_GREEN, TRAFFIC_GREEN, "10 GREEN / GREEN", false },
    { TRAFFIC_RED,   TRAFFIC_RED,   "11 RED / RED",     true }
};

#define TRAFFIC_STEP_COUNT (sizeof(traffic_steps) / sizeof(traffic_steps[0]))

static size_t next_step;

void driver_demo_init(UART_HandleTypeDef *uart)
{
    demo_uart = uart;
    traffic_init();
    next_step = 0U;
    demo_print("\r\nCO3009-10 traffic demo\r\n00 RED / RED\r\n");
    last_tick_ms = HAL_GetTick();
}

void driver_demo_run(void)
{
    uint32_t now = HAL_GetTick();
    if (next_step == TRAFFIC_STEP_COUNT || (now - last_tick_ms) < TRAFFIC_STEP_MS) {
        return;
    }

    const traffic_step_t *step = &traffic_steps[next_step++];
    const bool accepted = traffic_set_pair(step->road1, step->road2);
    last_tick_ms = now;
    demo_print(step->label);
    if (accepted != step->accepted) {
        traffic_init();
        next_step = TRAFFIC_STEP_COUNT;
        demo_print(" FAIL; reset to RED / RED\r\n");
        return;
    }
    demo_print(accepted ? "\r\n" : " rejected; pair preserved\r\n");
}

#else

#include <stdio.h>

#include "button.h"

void driver_demo_init(UART_HandleTypeDef *uart)
{
    demo_uart = uart;
    button_init();
    demo_print("\r\nCO3009-11 buttons: D7 D8 D9 D10 USER\r\n");
    last_tick_ms = HAL_GetTick();
}

void driver_demo_run(void)
{
    static const char *const names[BUTTON_COUNT] = { "D7", "D8", "D9", "D10", "USER" };
    const uint32_t now = HAL_GetTick();
    button_event_t event;
    if ((now - last_tick_ms) >= BUTTON_POLL_PERIOD_MS) {
        /* After a delay, take one fresh sample. Do not replay missed samples. */
        last_tick_ms = now;
        button_poll();
    }

    if (button_take_event(&event)) {
        char line[24];
        const char *type = event.type == BUTTON_EVENT_PRESS ? "PRESS" : "LONG_PRESS";
        snprintf(line, sizeof(line), "%s %s\r\n", names[event.id], type);
        demo_print(line);
    }
}

#endif
#endif
