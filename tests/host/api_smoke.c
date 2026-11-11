#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "button.h"
#include "dht20.h"
#include "fsm.h"
#include "lcd.h"
#include "mock_modules.h"
#include "scheduler.h"
#include "software_timer.h"
#include "traffic_light.h"

static void test_scheduler_button_fsm_and_traffic(void)
{
    SCH_Init();
    button_init();
    traffic_init();
    fsm_init();

    button_poll();
    mock_button_inject((button_event_t){BUTTON_D7, BUTTON_EVENT_PRESS});

    uint32_t task = SCH_Add_Task(fsm_run, 1, 0);
    assert(task != 0);

    SCH_Dispatch_Tasks();
    assert(mock_traffic_road1() == TRAFFIC_RED);
    assert(mock_traffic_road2() == TRAFFIC_RED);

    SCH_Update();
    SCH_Dispatch_Tasks();
    assert(mock_traffic_road1() == TRAFFIC_RED);
    assert(mock_traffic_road2() == TRAFFIC_GREEN);
    assert(SCH_Delete_Task(task) == 0);

    assert(!traffic_set_pair(TRAFFIC_GREEN, TRAFFIC_GREEN));
    assert(mock_traffic_road1() == TRAFFIC_RED);
    assert(mock_traffic_road2() == TRAFFIC_GREEN);

    assert(traffic_set_pair(TRAFFIC_RED, TRAFFIC_RED));
    mock_button_inject((button_event_t){BUTTON_D7, BUTTON_EVENT_PRESS});
    SCH_Dispatch_Tasks();
    assert(mock_traffic_road2() == TRAFFIC_RED);

    button_clear_events();
    button_event_t event;
    assert(!button_take_event(&event));
}

static void test_software_timer(void)
{
    sw_timer_init();
    assert(!sw_timer_start(SW_TIMER_TRAFFIC_PHASE, 0));
    assert(sw_timer_start(SW_TIMER_TRAFFIC_PHASE, 2));

    sw_timer_tick();
    assert(!sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE));

    sw_timer_tick();
    assert(sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE));
    assert(!sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE));

    assert(sw_timer_start(SW_TIMER_TRAFFIC_PHASE, 1));
    sw_timer_tick();
    assert(sw_timer_cancel(SW_TIMER_TRAFFIC_PHASE));
    assert(!sw_timer_take_timeout(SW_TIMER_TRAFFIC_PHASE));
}

static void test_dht20_and_lcd(void)
{
    dht20_sample_t sample = {0, 0};

    dht20_init();
    assert(dht20_get_latest(&sample) == DHT20_NO_SAMPLE);
    assert(sample.temperature_x10 == 0 && sample.humidity_x10 == 0);

    dht20_poll();
    assert(dht20_get_latest(&sample) == DHT20_OK);
    assert(sample.temperature_x10 == 253);
    assert(sample.humidity_x10 == 654);
    assert(dht20_get_latest(NULL) == DHT20_ERROR);

    assert(lcd_init());
    assert(lcd_write_line(0, "TIME: 100"));
    assert(lcd_write_line(0, "TIME: 9"));
    assert(strcmp(mock_lcd_line(0), "TIME: 9         ") == 0);
    assert(!lcd_write_line(2, "Invalid row"));
}

int main(void)
{
    test_scheduler_button_fsm_and_traffic();
    test_software_timer();
    test_dht20_and_lcd();

    puts("CO3009-02 API smoke test: PASS");
    return 0;
}
