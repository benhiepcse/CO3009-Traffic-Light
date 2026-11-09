#ifndef MOCK_MODULES_H
#define MOCK_MODULES_H

#include <stdint.h>

#include "button.h"
#include "traffic_light.h"

void mock_button_inject(button_event_t event);
traffic_color_t mock_traffic_road1(void);
traffic_color_t mock_traffic_road2(void);
const char *mock_lcd_line(uint8_t row);

#endif
