#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"

static unsigned writes;
static uint16_t length;
static uint8_t last_frame[80];
static bool bus_ok = true;
static unsigned probes;
static unsigned fail_write;
static uint8_t backpack = 0x3FU;
static uint8_t other_device;
static unsigned busy_polls;
static uint32_t clock_ms;
static uint32_t total_wait_ms;
static uint8_t startup_nibbles[5];
static uint32_t startup_times[10];

bool lcd_test_bus_busy(void)
{
    if (busy_polls == 0U) {
        return false;
    }
    --busy_polls;
    ++clock_ms;
    return true;
}

bool lcd_test_probe(uint16_t address)
{
    assert((address & 1U) == 0U);
    const uint8_t addr = (uint8_t)(address >> 1U);
    assert((addr >= 0x20U && addr <= 0x27U) || (addr >= 0x39U && addr <= 0x3FU));
#ifdef CO3009_LCD_ADDRESS
    assert(addr == CO3009_LCD_ADDRESS);
#endif
    ++probes;
    return bus_ok && (addr == backpack || addr == other_device);
}

uint32_t lcd_test_now_ms(void)
{
    return clock_ms;
}

bool lcd_test_write(uint16_t address, const uint8_t *data, uint16_t len,
                    uint32_t timeout_ms)
{
    assert(address == (uint16_t)(backpack << 1U) && address != 0x70U);
    assert(len <= sizeof(last_frame) && timeout_ms <= 8U);
    ++writes;
    length = writes == fail_write && len > 2U ? 2U : len;
    memcpy(last_frame, data, length);
    if (writes <= 5U) {
        startup_nibbles[writes - 1U] = data[len - 1U] & 0xF0U;
    }
    if (writes <= 10U) {
        startup_times[writes - 1U] = clock_ms;
    }
    return bus_ok && writes != fail_write;
}

void lcd_test_wait_ms(uint32_t wait_ms)
{
    total_wait_ms += wait_ms;
    clock_ms += wait_ms;
}

static void poll_until_ready(void)
{
    const uint32_t wait_before = total_wait_ms;
    for (unsigned tick = 0U; tick < 50U; ++tick) {
        clock_ms += 50U;
        const unsigned transfers = writes + probes;
        bool ready = lcd_poll_ready();
        assert(writes + probes <= transfers + 1U && total_wait_ms == wait_before);
        if (ready) {
            return;
        }
    }
    assert(0 && "LCD failed to recover");
}

/* Reassemble LCD bytes by sampling each falling edge of Enable. */
static size_t decode(uint8_t *values, bool *is_data)
{
    uint8_t prev = 0U;
    uint8_t high = 0U;
    bool have_high = false;
    size_t count = 0U;
    for (uint16_t i = 0U; i < length; ++i) {
        uint8_t pins = last_frame[i];
        assert((pins & 0x0AU) == 0x08U); /* Backlight on, R/W low. */
        if ((prev & 0x04U) != 0U && (pins & 0x04U) == 0U) {
            assert((prev ^ pins) == 0x04U);
            if (have_high) {
                values[count] = high | (pins >> 4U);
                is_data[count++] = (pins & 1U) != 0U;
            } else {
                high = pins & 0xF0U;
            }
            have_high = !have_high;
        }
        prev = pins;
    }
    assert(!have_high);
    return count;
}

static void expect_line(uint8_t start, const char *expected)
{
    uint8_t values[20];
    bool data[20];
    assert(decode(values, data) == LCD_COLUMNS + 1U);
    assert(values[0] == start && !data[0]);
    assert(memcmp(&values[1], expected, LCD_COLUMNS) == 0);
    for (size_t i = 1U; i <= LCD_COLUMNS; ++i) {
        assert(data[i]);
    }
}

int main(void)
{
#ifdef CO3009_LCD_ADDRESS
    backpack = CO3009_LCD_ADDRESS;
#endif
    assert(!lcd_write_line(0U, "before init") && writes == 0U);
    assert(lcd_init());
    const uint8_t init_sequence[] = { 0, 0x30, 0x30, 0x30, 0x20 };
    assert(memcmp(init_sequence, startup_nibbles, sizeof(init_sequence)) == 0);
    assert(total_wait_ms >= 50U);
    assert(startup_times[0] >= 50U);
    assert(startup_times[2] - startup_times[1] >= 5U);
    assert(startup_times[3] - startup_times[2] >= 1U);
    assert(startup_times[8] - startup_times[7] >= 2U);
    const unsigned initial_writes = writes;
    assert(!lcd_write_line(LCD_ROWS, "bad row"));
    assert(!lcd_write_line(0U, NULL));
    assert(!lcd_set_cursor(0U, LCD_COLUMNS));
    assert(writes == initial_writes);
    assert(lcd_write_line(0U, "ABCDEFGHIJKLMNOPextra"));
    assert(writes == initial_writes + 1U);
    expect_line(0x80U, "ABCDEFGHIJKLMNOP");
    assert(lcd_write_line(0U, "short"));
    expect_line(0x80U, "short           ");
    busy_polls = 1U;  /* Previous STOP is still on the wire when the next write starts. */
    assert(lcd_write_line(1U, ""));
    expect_line(0xC0U, "                ");
    puts("PASS: LCD I2C address, initialization, bounds and full-line padding");

    uint8_t raw[20];
    bool data[20];
    assert(lcd_set_cursor(1U, 2U));
    assert(decode(raw, data) == 1U && raw[0] == 0xC2U && !data[0]);
    assert(lcd_print_int(INT32_MIN));
    assert(decode(raw, data) == 11U && memcmp(raw, "-2147483648", 11U) == 0);
    assert(lcd_print("abcXYZ"));
    assert(decode(raw, data) == 3U && memcmp(raw, "abc", 3U) == 0);
    assert(lcd_clear());
    assert(decode(raw, data) == 1U && raw[0] == 0x01U && !data[0]);
    clock_ms = UINT32_MAX - 200U;
    fail_write = writes + 1U;  /* A failed frame may already have clocked a nibble. */
    assert(!lcd_write_line(0U, "interrupted"));
    assert(length == 2U);
    assert(!lcd_print("needs init"));
    const unsigned before_retry = probes;
    clock_ms += 499U;
    assert(!lcd_poll_ready() && probes == before_retry);
    poll_until_ready();
    assert(lcd_write_line(1U, "recovered"));
    expect_line(0xC0U, "recovered       ");

    const unsigned before_absent = writes;
    bus_ok = false;
    assert(!lcd_init() && writes == before_absent);
#ifndef CO3009_LCD_ADDRESS
    backpack = 0x21U;
#endif
    bus_ok = true;
    poll_until_ready();  /* Power arrives after the MCU's startup attempt. */
    assert(lcd_write_line(0U, "START"));
    expect_line(0x80U, "START           ");

    fail_write = writes + 4U;  /* Lose a transfer during the sync sequence itself. */
    assert(!lcd_init());
    poll_until_ready();
    assert(lcd_write_line(1U, "ALL RED"));
    expect_line(0xC0U, "ALL RED         ");

    busy_polls = 10U;  /* Stuck bus must yield to the traffic controller. */
    const uint32_t busy_start = clock_ms;
    const unsigned before_busy = writes;
    assert(!lcd_write_line(0U, "busy"));
    assert(clock_ms - busy_start == 2U && writes == before_busy);
    busy_polls = 0U;
    poll_until_ready();

#ifndef CO3009_LCD_ADDRESS
    other_device = 0x27U;
    const unsigned before_ambiguous = writes;
    assert(!lcd_init() && writes == before_ambiguous);
    other_device = 0U;
    poll_until_ready();
#endif
    puts("PASS: LCD address selection, startup failure and nonblocking resync/redraw");
    return 0;
}
