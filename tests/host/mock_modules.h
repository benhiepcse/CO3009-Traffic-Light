#ifndef MOCK_MODULES_H
#define MOCK_MODULES_H

#include <stdint.h>

#include "button.h"
#include "dht20.h"
#include "traffic_light.h"

void mock_button_inject(button_event_t event);
void mock_fail_next_timer_start(void);
void mock_fail_next_traffic_pair(void);
void mock_fail_next_lcd_write(void);
void mock_dht20_sample(dht20_status_t status, int16_t temperature_x10,
                        uint16_t humidity_x10);
traffic_color_t mock_traffic_road1(void);
traffic_color_t mock_traffic_road2(void);
uint32_t mock_traffic_pair_requests(void);
uint32_t mock_traffic_dual_green_requests(void);
uint32_t mock_lcd_write_requests(void);
const char *mock_lcd_line(uint8_t row);

#endif
