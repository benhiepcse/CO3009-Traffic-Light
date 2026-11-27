#include "lcd.h"

#include <stddef.h>

#include "main.h"

extern I2C_HandleTypeDef hi2c1;

/* Confirm this 7-bit address on the actual LCD backpack. */
#define LCD_I2C_ADDRESS_7BIT 0x27U
#define LCD_I2C_HAL_ADDRESS (LCD_I2C_ADDRESS_7BIT << 1U)

#define LCD_ROWS             2U
#define LCD_COLUMNS         16U
#define LCD_I2C_TIMEOUT_MS  15U

/*
 * PCF8574 backpack pin map:
 * P0 = RS, P1 = RW, P2 = EN, P3 = backlight.
 * P4 through P7 carry LCD data bits D4 through D7.
 * RW stays low for all writes.
 */
#define LCD_RS        0x01U
#define LCD_ENABLE    0x04U
#define LCD_BACKLIGHT 0x08U

#define LCD_NIBBLE_BYTES 3U
#define LCD_BYTE_BYTES   (2U * LCD_NIBBLE_BYTES)

static bool initialized;

static void encode_nibble(uint8_t *buffer, uint8_t nibble, uint8_t flags)
{
    const uint8_t output = (uint8_t)(
        (nibble & 0xF0U) | flags | LCD_BACKLIGHT
    );

    buffer[0] = output;
    buffer[1] = (uint8_t)(output | LCD_ENABLE);
    buffer[2] = output;
}

static void encode_byte(uint8_t *buffer, uint8_t value, uint8_t flags)
{
    encode_nibble(buffer, value, flags);
    encode_nibble(buffer + LCD_NIBBLE_BYTES,
                  (uint8_t)(value << 4U), flags);
}

static bool send_buffer(uint8_t *buffer, uint16_t length)
{
    return HAL_I2C_Master_Transmit(&hi2c1, LCD_I2C_HAL_ADDRESS,
                                   buffer, length,
                                   LCD_I2C_TIMEOUT_MS) == HAL_OK;
}

static bool send_init_nibble(uint8_t nibble)
{
    uint8_t buffer[LCD_NIBBLE_BYTES];

    encode_nibble(buffer, nibble, 0U);
    return send_buffer(buffer, (uint16_t)sizeof buffer);
}

static bool send_command(uint8_t command)
{
    uint8_t buffer[LCD_BYTE_BYTES];

    encode_byte(buffer, command, 0U);
    return send_buffer(buffer, (uint16_t)sizeof buffer);
}

bool lcd_init(void)
{
    initialized = false;

    /* Startup only. Run this before starting the scheduler. */
    HAL_Delay(50U);

    if (!send_init_nibble(0x30U)) {
        return false;
    }
    HAL_Delay(5U);

    if (!send_init_nibble(0x30U)) {
        return false;
    }
    HAL_Delay(1U);

    if (!send_init_nibble(0x30U)) {
        return false;
    }
    HAL_Delay(1U);

    if (!send_init_nibble(0x20U)) {
        return false;
    }

    if (!send_command(0x28U)   /* 4-bit mode, two lines */
        || !send_command(0x08U) /* Display off during setup */
        || !send_command(0x01U)) { /* Clear display */
        return false;
    }
    HAL_Delay(2U);

    if (!send_command(0x06U)   /* Increment cursor after each character */
        || !send_command(0x0CU)) { /* Display on, cursor off */
        return false;
    }

    initialized = true;
    return true;
}

bool lcd_write_line(uint8_t row, const char *text)
{
    static const uint8_t row_address[LCD_ROWS] = { 0x00U, 0x40U };
    uint8_t buffer[(LCD_COLUMNS + 1U) * LCD_BYTE_BYTES];

    if (!initialized || row >= LCD_ROWS || text == NULL) {
        return false;
    }

    /* First send the cursor command, then all 16 characters. */
    encode_byte(buffer, (uint8_t)(0x80U | row_address[row]), 0U);

    for (size_t column = 0U; column < LCD_COLUMNS; ++column) {
        uint8_t character = (uint8_t)' ';

        if (*text != '\0') {
            character = (uint8_t)*text;
            ++text;
        }

        encode_byte(buffer + (column + 1U) * LCD_BYTE_BYTES,
                    character, LCD_RS);
    }

    return send_buffer(buffer, (uint16_t)sizeof buffer);
}
