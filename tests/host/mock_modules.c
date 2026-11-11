#include "mock_modules.h"

#include <stddef.h>
#include <string.h>

#include "dht20.h"
#include "fsm.h"
#include "lcd.h"
#include "scheduler.h"
#include "software_timer.h"

static void (*scheduled_task)(void);
static uint32_t task_delay;
static uint32_t task_period;
static uint32_t task_id;
static uint32_t next_task_id;

void SCH_Init(void)
{
    scheduled_task = NULL;
    task_delay = 0;
    task_period = 0;
    task_id = 0;
    next_task_id = 1;
}

uint32_t SCH_Add_Task(void (*task)(void),
                      uint32_t delay_ticks,
                      uint32_t period_ticks)
{
    if (task == NULL || scheduled_task != NULL) {
        return 0;
    }

    scheduled_task = task;
    task_delay = delay_ticks;
    task_period = period_ticks;
    task_id = next_task_id++;

    if (next_task_id == 0) {
        next_task_id = 1;
    }

    return task_id;
}

uint8_t SCH_Delete_Task(uint32_t id)
{
    if (scheduled_task == NULL || id != task_id) {
        return 0;
    }

    scheduled_task = NULL;
    task_id = 0;
    return 1;
}

void SCH_Update(void)
{
    if (scheduled_task != NULL && task_delay > 0) {
        --task_delay;
    }
}

void SCH_Dispatch_Tasks(void)
{
    if (scheduled_task == NULL || task_delay != 0) {
        return;
    }

    void (*run)(void) = scheduled_task;

    if (task_period == 0) {
        scheduled_task = NULL;
        task_id = 0;
    } else {
        task_delay = task_period;
    }

    run();
}

static uint32_t timer_remaining;
static bool timer_expired;
static bool timer_start_fails;

void sw_timer_init(void)
{
    timer_remaining = 0;
    timer_expired = false;
    timer_start_fails = false;
}

void sw_timer_tick(void)
{
    if (timer_remaining > 0) {
        --timer_remaining;
        if (timer_remaining == 0) {
            timer_expired = true;
        }
    }
}

bool sw_timer_start(sw_timer_id_t id, uint32_t duration_ticks)
{
    if (id != SW_TIMER_TRAFFIC_PHASE || duration_ticks == 0) {
        return false;
    }
    if (timer_start_fails) {
        timer_start_fails = false;
        return false;
    }

    timer_remaining = duration_ticks;
    timer_expired = false;
    return true;
}

bool sw_timer_cancel(sw_timer_id_t id)
{
    if (id != SW_TIMER_TRAFFIC_PHASE) {
        return false;
    }

    sw_timer_init();
    return true;
}

bool sw_timer_take_timeout(sw_timer_id_t id)
{
    if (id != SW_TIMER_TRAFFIC_PHASE || !timer_expired) {
        return false;
    }

    timer_expired = false;
    return true;
}

void mock_fail_next_timer_start(void)
{
    timer_start_fails = true;
}

enum { MOCK_BUTTON_CAPACITY = 8 };
static button_event_t button_events[MOCK_BUTTON_CAPACITY];
static unsigned button_head;
static unsigned button_count;

void button_init(void)
{
    button_head = 0;
    button_count = 0;
}

void button_poll(void)
{
}

void mock_button_inject(button_event_t event)
{
    if (button_count < MOCK_BUTTON_CAPACITY) {
        button_events[(button_head + button_count) % MOCK_BUTTON_CAPACITY] = event;
        ++button_count;
    }
}

bool button_take_event(button_event_t *out)
{
    if (out == NULL || button_count == 0) {
        return false;
    }

    *out = button_events[button_head];
    button_head = (button_head + 1) % MOCK_BUTTON_CAPACITY;
    --button_count;
    return true;
}

void button_clear_events(void)
{
    button_head = 0;
    button_count = 0;
}

static traffic_color_t road1_color;
static traffic_color_t road2_color;
static uint32_t pair_requests;
static uint32_t dual_green_requests;
static bool traffic_pair_fails;

void traffic_init(void)
{
    road1_color = TRAFFIC_RED;
    road2_color = TRAFFIC_RED;
    pair_requests = 0;
    dual_green_requests = 0;
    traffic_pair_fails = false;
}

static bool traffic_color_is_valid(traffic_color_t color)
{
    return color == TRAFFIC_OFF || color == TRAFFIC_RED
        || color == TRAFFIC_AMBER || color == TRAFFIC_GREEN;
}

bool traffic_set_pair(traffic_color_t road1, traffic_color_t road2)
{
    ++pair_requests;
    if (road1 == TRAFFIC_GREEN && road2 == TRAFFIC_GREEN) {
        ++dual_green_requests;
        return false;
    }
    if (!traffic_color_is_valid(road1) || !traffic_color_is_valid(road2)) {
        return false;
    }
    if (traffic_pair_fails) {
        traffic_pair_fails = false;
        return false;
    }

    road1_color = road1;
    road2_color = road2;
    return true;
}

traffic_color_t mock_traffic_road1(void)
{
    return road1_color;
}

traffic_color_t mock_traffic_road2(void)
{
    return road2_color;
}

uint32_t mock_traffic_pair_requests(void)
{
    return pair_requests;
}

uint32_t mock_traffic_dual_green_requests(void)
{
    return dual_green_requests;
}

void mock_fail_next_traffic_pair(void)
{
    traffic_pair_fails = true;
}

static dht20_status_t sensor_status;
static dht20_sample_t sensor_sample;

void dht20_init(void)
{
    sensor_status = DHT20_NO_SAMPLE;
    sensor_sample = (dht20_sample_t){ 253, 654 };
}

void dht20_poll(void)
{
    sensor_status = DHT20_OK;
}

dht20_status_t dht20_get_latest(dht20_sample_t *out)
{
    if (out == NULL) {
        return DHT20_ERROR;
    }

    if (sensor_status == DHT20_OK) {
        *out = sensor_sample;
    }

    return sensor_status;
}

void mock_dht20_sample(dht20_status_t status, int16_t temperature_x10,
                        uint16_t humidity_x10)
{
    sensor_status = status;
    sensor_sample = (dht20_sample_t){ temperature_x10, humidity_x10 };
}

enum {
    MOCK_LCD_ROWS = 2,
    MOCK_LCD_COLUMNS = 16
};

static char lcd_lines[MOCK_LCD_ROWS][MOCK_LCD_COLUMNS + 1];
static bool lcd_ready;
static bool lcd_write_fails;
static uint32_t lcd_write_requests;

bool lcd_init(void)
{
    for (size_t row = 0; row < MOCK_LCD_ROWS; ++row) {
        memset(lcd_lines[row], ' ', MOCK_LCD_COLUMNS);
        lcd_lines[row][MOCK_LCD_COLUMNS] = '\0';
    }

    lcd_ready = true;
    lcd_write_fails = false;
    lcd_write_requests = 0;
    return true;
}

bool lcd_write_line(uint8_t row, const char *text)
{
    ++lcd_write_requests;
    if (!lcd_ready || row >= MOCK_LCD_ROWS || text == NULL) {
        return false;
    }
    if (lcd_write_fails) {
        lcd_write_fails = false;
        return false;
    }

    size_t column = 0;
    while (column < MOCK_LCD_COLUMNS && text[column] != '\0') {
        lcd_lines[row][column] = text[column];
        ++column;
    }

    memset(&lcd_lines[row][column], ' ', MOCK_LCD_COLUMNS - column);
    return true;
}

const char *mock_lcd_line(uint8_t row)
{
    if (!lcd_ready || row >= MOCK_LCD_ROWS) {
        return NULL;
    }

    return lcd_lines[row];
}

uint32_t mock_lcd_write_requests(void)
{
    return lcd_write_requests;
}

void mock_fail_next_lcd_write(void)
{
    lcd_write_fails = true;
}

#ifndef MOCK_MODULES_USE_REAL_FSM
void fsm_init(void)
{
    (void)traffic_set_pair(TRAFFIC_RED, TRAFFIC_RED);
}

void fsm_run(void)
{
    button_event_t event;

    if (button_take_event(&event)
        && event.id == BUTTON_D7
        && event.type == BUTTON_EVENT_PRESS) {
        (void)traffic_set_pair(TRAFFIC_RED, TRAFFIC_GREEN);
    }
}
#endif
