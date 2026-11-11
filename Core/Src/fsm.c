#include "fsm.h"

#include <stdio.h>
#include <string.h>

#include "button.h"
#include "dht20.h"
#include "fsm_automatic.h"
#include "fsm_manual.h"
#include "fsm_tuning.h"
#include "global.h"
#include "lcd.h"
#include "software_timer.h"

static system_mode_t target_mode;
static traffic_timing_t pending_timing;
static char last_screen[FSM_LCD_ROWS][FSM_LCD_COLUMNS + 1];
static bool screen_valid[FSM_LCD_ROWS];

static const char *mode_name(system_mode_t mode)
{
    switch (mode) {
    case SYSTEM_AUTOMATIC:
        return "AUTO";
    case SYSTEM_MANUAL:
        return "MANUAL";
    default:
        return "TUNING";
    }
}

static char color_letter(traffic_color_t color)
{
    static const char letters[] = { '-', 'R', 'Y', 'G' };
    if ((unsigned)color > TRAFFIC_GREEN) {
        return '?';
    }
    return letters[color];
}

static void format_environment(char line[FSM_LCD_COLUMNS + 1])
{
    dht20_sample_t sample;
    const dht20_status_t status = dht20_get_latest(&sample);
    if (status != DHT20_OK) {
        const char *message = "WAIT SENSOR";
        if (status == DHT20_ERROR) {
            message = "SENSOR ERROR";
        }
        (void)snprintf(line, FSM_LCD_COLUMNS + 1, "%s", message);
        return;
    }

    int32_t temperature = sample.temperature_x10;
    const char *sign = "";
    if (temperature < 0) {
        sign = "-";
        temperature = -temperature;
    }
    (void)snprintf(line, FSM_LCD_COLUMNS + 1, "T:%s%ld.%ld H:%u.%u", sign,
                   (long)(temperature / 10), (long)(temperature % 10),
                   (unsigned)(sample.humidity_x10 / 10),
                   (unsigned)(sample.humidity_x10 % 10));
}

static void update_screen(void)
{
    char lines[FSM_LCD_ROWS][FSM_LCD_COLUMNS + 1];
    switch (system_mode) {
    case SYSTEM_AUTOMATIC:
    case SYSTEM_MANUAL:
        (void)snprintf(lines[0], sizeof lines[0], "%s %c/%c", mode_name(system_mode),
                       color_letter(current_road1), color_letter(current_road2));
        format_environment(lines[1]);
        break;
    case SYSTEM_TUNING:
        fsm_tuning_format(lines);
        break;
    case SYSTEM_BOOT:
        (void)snprintf(lines[0], sizeof lines[0], "START");
        (void)snprintf(lines[1], sizeof lines[1], "ALL RED");
        break;
    case SYSTEM_SWITCH_AMBER:
    case SYSTEM_SWITCH_RED:
        (void)snprintf(lines[0], sizeof lines[0], "SWITCHING");
        (void)snprintf(lines[1], sizeof lines[1], "TO %s", mode_name(target_mode));
        break;
    default:
        (void)snprintf(lines[0], sizeof lines[0], "STOP");
        (void)snprintf(lines[1], sizeof lines[1], "CHECK MODULES");
        break;
    }

    for (uint8_t row = 0; row < FSM_LCD_ROWS; ++row) {
        if (!screen_valid[row] || strcmp(last_screen[row], lines[row]) != 0) {
            if (lcd_write_line(row, lines[row])) {
                (void)strcpy(last_screen[row], lines[row]);
                screen_valid[row] = true;
            }
        }
    }
}

static button_id_t take_button(void)
{
    button_id_t selected = BUTTON_COUNT;
    button_event_t event;
    while (button_take_event(&event)) {
        if (event.type != BUTTON_EVENT_PRESS) {
            continue;
        }
        const bool actionable = event.id == BUTTON_D7
            || (system_mode == SYSTEM_MANUAL && event.id == BUTTON_D8)
            || (system_mode == SYSTEM_TUNING
                && event.id >= BUTTON_D8 && event.id <= BUTTON_D10);
        if (actionable && event.id < selected) {
            selected = event.id;
        }
    }
    return selected;
}

static void enter_mode(system_mode_t mode)
{
    system_mode = mode;
    button_clear_events();
    switch (mode) {
    case SYSTEM_AUTOMATIC:
        active_timing = pending_timing;
        fsm_automatic_enter();
        break;
    case SYSTEM_MANUAL:
        fsm_manual_enter();
        break;
    default:
        fsm_tuning_enter();
        break;
    }
}

static void begin_switch(system_mode_t target)
{
    target_mode = target;
    button_clear_events();
    if (current_road1 == TRAFFIC_GREEN || current_road1 == TRAFFIC_AMBER) {
        system_mode = SYSTEM_SWITCH_AMBER;
        (void)fsm_set_phase(TRAFFIC_AMBER, TRAFFIC_RED,
                            active_timing.amber_seconds * TRAFFIC_TICKS_PER_SECOND);
    } else if (current_road2 == TRAFFIC_GREEN || current_road2 == TRAFFIC_AMBER) {
        system_mode = SYSTEM_SWITCH_AMBER;
        (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_AMBER,
                            active_timing.amber_seconds * TRAFFIC_TICKS_PER_SECOND);
    } else {
        system_mode = SYSTEM_SWITCH_RED;
        (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, FSM_ALL_RED_TICKS);
    }
}

void fsm_init(void)
{
    global_reset();
    target_mode = SYSTEM_AUTOMATIC;
    pending_timing = active_timing;
    (void)memset(screen_valid, 0, sizeof screen_valid);
    button_clear_events();
    (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, FSM_ALL_RED_TICKS);
    update_screen();
}

void fsm_run(void)
{
    button_id_t button = BUTTON_COUNT;
    if (system_mode == SYSTEM_AUTOMATIC || system_mode == SYSTEM_MANUAL
        || system_mode == SYSTEM_TUNING) {
        button = take_button();
    } else {
        button_clear_events();
    }

    switch (system_mode) {
    case SYSTEM_BOOT:
        if (sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
            enter_mode(SYSTEM_AUTOMATIC);
        }
        break;
    case SYSTEM_AUTOMATIC:
        if (button == BUTTON_D7) {
            begin_switch(SYSTEM_MANUAL);
        } else {
            fsm_automatic_run();
        }
        break;
    case SYSTEM_MANUAL:
        if (button == BUTTON_D7) {
            begin_switch(SYSTEM_TUNING);
        } else {
            fsm_manual_run(button == BUTTON_D8);
        }
        break;
    case SYSTEM_TUNING:
        if (fsm_tuning_run(button, &pending_timing)) {
            begin_switch(SYSTEM_AUTOMATIC);
        }
        break;
    case SYSTEM_SWITCH_AMBER:
        if (sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
            system_mode = SYSTEM_SWITCH_RED;
            (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, FSM_ALL_RED_TICKS);
        }
        break;
    case SYSTEM_SWITCH_RED:
        if (sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE)) {
            enter_mode(target_mode);
        }
        break;
    case SYSTEM_FAULT:
        break;
    default:
        system_mode = SYSTEM_FAULT;
        (void)fsm_set_phase(TRAFFIC_RED, TRAFFIC_RED, 0);
        break;
    }
    update_screen();
}
