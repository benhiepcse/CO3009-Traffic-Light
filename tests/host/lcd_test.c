#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "lcd.h"

static unsigned writes;
static uint16_t length;
static uint8_t last_frame[80];
static bool bus_ok = true;
static bool partial_failure;
static uint32_t total_wait_ms;
static uint8_t startup_nibbles[5];

bool lcd_test_write(uint16_t address, const uint8_t *data, uint16_t len,
                    uint32_t timeout_ms, bool *partial)
{
    assert(address == 0x7EU && len <= sizeof(last_frame) && timeout_ms <= 8U);
    ++writes;
    length = len;
    memcpy(last_frame, data, len);
    if (writes <= 5U) {
        startup_nibbles[writes - 1U] = data[len - 1U] & 0xF0U;
    }
    *partial = partial_failure;
    return bus_ok;
}

void lcd_test_wait_ms(uint32_t wait_ms)
{
    total_wait_ms += wait_ms;
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
    assert(!lcd_write_line(0U, "before init") && writes == 0U);
    assert(lcd_init());
    const uint8_t init_sequence[] = { 0, 0x30, 0x30, 0x30, 0x20 };
    assert(memcmp(init_sequence, startup_nibbles, sizeof(init_sequence)) == 0);
    assert(total_wait_ms >= 50U);
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
    bus_ok = false;
    assert(!lcd_write_line(0U, "I2C error"));
    bus_ok = true;
    assert(lcd_write_line(0U, "retry worked"));
    bus_ok = false;
    partial_failure = true;
    assert(!lcd_write_line(0U, "partial I2C error"));
    partial_failure = false;
    bus_ok = true;
    assert(!lcd_print("needs init"));
    assert(lcd_init() && lcd_write_line(1U, "recovered"));
    puts("PASS: LCD cursor, integer, clear, I2C retry and partial-frame re-init");
    return 0;
}
