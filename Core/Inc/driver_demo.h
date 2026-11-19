#ifndef DRIVER_DEMO_H
#define DRIVER_DEMO_H

#include "stm32f1xx_hal.h"

/* Compiled for CO3009_TRAFFIC_DEMO or CO3009_BUTTON_DEMO. */
void driver_demo_init(UART_HandleTypeDef *uart);
void driver_demo_run(void);

#endif
