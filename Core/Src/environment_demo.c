#include "environment_demo.h"

#if defined(CO3009_DHT20_DEMO) || defined(CO3009_LCD_DEMO)

#include <string.h>

#include "main.h"
#include "scheduler.h"
#include "software_timer.h"

static UART_HandleTypeDef *demo_uart;

static void print_log(const char *text)
{
    HAL_UART_Transmit(demo_uart, (uint8_t *)text, (uint16_t)strlen(text), 5U);
}

#ifdef CO3009_DHT20_DEMO

#include <stdio.h>

#include "dht20.h"

static void print_sample(void)
{
    dht20_sample_t sample;
    const dht20_status_t status = dht20_get_latest(&sample);
    if (status == DHT20_NO_SAMPLE) {
        print_log("DHT20 waiting for sample\r\n");
    } else if (status == DHT20_ERROR) {
        print_log("DHT20 timeout or invalid sample\r\n");
    } else {
        const int temp = sample.temperature_x10;
        const unsigned magnitude = (unsigned)(temp < 0 ? -temp : temp);
        const unsigned humidity = sample.humidity_x10;
        char line[48];
        snprintf(line, sizeof(line), "T=%s%u.%u C  RH=%u.%u %%\r\n",
                 temp < 0 ? "-" : "", magnitude / 10U, magnitude % 10U,
                 humidity / 10U, humidity % 10U);
        print_log(line);
    }
}

void environment_demo_init(UART_HandleTypeDef *uart)
{
    demo_uart = uart;
    print_log("\r\nCO3009-16 DHT20 7-bit address 0x38\r\n");
    dht20_init();
    SCH_Init();
    if (SCH_Add_Task(dht20_poll, 1U, SW_TIMER_MS_TO_TICKS(DHT20_POLL_MS)) == 0U ||
        SCH_Add_Task(print_sample, SW_TIMER_MS_TO_TICKS(250U),
                     SW_TIMER_MS_TO_TICKS(DHT20_PERIOD_MS)) == 0U) {
        Error_Handler();
    }
}

#else

#include "lcd.h"

#define SCREEN_PERIOD_MS 2000U

/* One task owns the display. Tuning pages displace the environment pages. */
static const char *const demo_screens[][LCD_ROWS] = {
    { "T: 25.3 C", "RH: 65.4 %" },
    { "TUNE GREEN", "60 seconds" },
    { "TUNE GREEN", "5 s" },
    { "T: 26.1 C", "RH: 64.9 %" }
};
static uint32_t page_start_ms;
static uint8_t page;
static uint8_t next_row;

static void show_demo_screen(void)
{
    if (!lcd_poll_ready()) {
        next_row = 0U;
        return;
    }
    const uint32_t now = HAL_GetTick();
    if (now - page_start_ms >= SCREEN_PERIOD_MS) {
        page_start_ms = now;
        page = (uint8_t)((page + 1U) % (sizeof(demo_screens) / sizeof(demo_screens[0])));
        next_row = 0U;
    }
    if (next_row < LCD_ROWS && lcd_write_line(next_row, demo_screens[page][next_row])) {
        ++next_row;
    }
}

void environment_demo_init(UART_HandleTypeDef *uart)
{
    demo_uart = uart;
    print_log("\r\nCO3009-17 LCD demo with simulated data\r\n");
    SCH_Init();
    if (!lcd_init()) {
        print_log("LCD offline, retrying. Check I2C wiring/address.\r\n");
    }
    page = 0U;
    next_row = 0U;
    page_start_ms = HAL_GetTick();
    if (SCH_Add_Task(show_demo_screen, 1U, 1U) == 0U) {
        Error_Handler();
    }
}

#endif
#endif
