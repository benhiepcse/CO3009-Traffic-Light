#ifndef ENVIRONMENT_DEMO_H
#define ENVIRONMENT_DEMO_H

#include "stm32f1xx_hal.h"

/* Registers the selected demo before TIM2 starts. */
void environment_demo_init(UART_HandleTypeDef *uart);

#endif
