#include "lcd.h"

#include <stddef.h>

#ifdef LCD_HOST_TEST
extern bool lcd_test_write(uint16_t address, const uint8_t *data, uint16_t len,
                           uint32_t timeout_ms, bool *partial);
extern void lcd_test_wait_ms(uint32_t wait_ms);
#else
#include "main.h"
extern I2C_HandleTypeDef hi2c1;
#endif

/* HD44780 1602 on the common PCF8574 backpack. R/W (P1) remains low. */
#define PIN_RS        0x01U
#define PIN_ENABLE    0x04U
#define PIN_BACKLIGHT 0x08U
#define CLEAR_DISPLAY 0x01U
#define DDRAM_ADDRESS 0x80U
#define I2C_TIMEOUT_MS 8U
#define ENCODED_BYTE   4U
#define LINE_FRAME_SIZE (2U + (1U + LCD_COLUMNS) * ENCODED_BYTE)

_Static_assert((CO3009_LCD_ADDRESS >= 0x20U && CO3009_LCD_ADDRESS <= 0x27U) ||
               (CO3009_LCD_ADDRESS >= 0x39U && CO3009_LCD_ADDRESS <= 0x3FU),
               "Select a PCF8574(A) address distinct from DHT20 (0x38)");

static bool ready;
static uint8_t cursor_column;

static bool transmit(uint8_t *data, uint16_t len)
{
    const uint16_t address = (uint16_t)(CO3009_LCD_ADDRESS << 1U);
#ifdef LCD_HOST_TEST
    bool partial = false;
    const bool ok = lcd_test_write(address, data, len, I2C_TIMEOUT_MS, &partial);
#else
    if (__HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY)) {
        return false;
    }
    const HAL_StatusTypeDef result =
        HAL_I2C_Master_Transmit(&hi2c1, address, data, len, I2C_TIMEOUT_MS);
    const bool ok = result == HAL_OK;
    /* F1 HAL decrements XferCount only after putting a data byte on the bus. */
    const bool partial = result != HAL_BUSY && hi2c1.XferCount < len;
#endif
    if (!ok && partial) {
        /* A partial frame may leave the controller between nibbles. */
        ready = false;
    }
    return ok;
}

static void wait_ms(uint32_t ms)
{
#ifdef LCD_HOST_TEST
    lcd_test_wait_ms(ms);
#else
    const uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < ms) {
        /* Used for startup and the explicit clear instruction only. */
    }
#endif
}

static bool init_nibble(uint8_t high)
{
    uint8_t frame[] = { high | PIN_BACKLIGHT | PIN_ENABLE, high | PIN_BACKLIGHT };
    return transmit(frame, sizeof(frame));
}

static void encode_byte(uint8_t out[ENCODED_BYTE], uint8_t value, uint8_t control)
{
    const uint8_t high = (value & 0xF0U) | control;
    const uint8_t low = ((uint8_t)(value << 4U)) | control;
    out[0] = high | PIN_ENABLE;
    out[1] = high;
    out[2] = low | PIN_ENABLE;
    out[3] = low;
}

static bool command(uint8_t value)
{
    uint8_t frame[1U + ENCODED_BYTE] = { PIN_BACKLIGHT };
    encode_byte(&frame[1], value, PIN_BACKLIGHT);
    return transmit(frame, sizeof(frame));
}

static uint8_t cursor_address(uint8_t row, uint8_t column)
{
    return (uint8_t)(DDRAM_ADDRESS | (row == 0U ? 0U : 0x40U) | column);
}

bool lcd_init(void)
{
    ready = false;
    wait_ms(50U);
    uint8_t idle = PIN_BACKLIGHT;
    if (!transmit(&idle, 1U) || !init_nibble(0x30U)) {
        return false;
    }
    wait_ms(6U);
    if (!init_nibble(0x30U)) {
        return false;
    }
    wait_ms(2U);
    if (!init_nibble(0x30U) || !init_nibble(0x20U) ||
        !command(0x28U) || !command(0x08U) || !command(CLEAR_DISPLAY)) {
        return false;
    }
    wait_ms(3U);
    ready = command(0x06U) && command(0x0CU);
    cursor_column = 0U;
    return ready;
}

bool lcd_clear(void)
{
    if (!ready || !command(CLEAR_DISPLAY)) {
        return false;
    }
    wait_ms(3U);
    cursor_column = 0U;
    return true;
}

bool lcd_set_cursor(uint8_t row, uint8_t column)
{
    if (!ready || row >= LCD_ROWS || column >= LCD_COLUMNS ||
        !command(cursor_address(row, column))) {
        return false;
    }
    cursor_column = column;
    return true;
}

bool lcd_print(const char *text)
{
    if (!ready || text == NULL) {
        return false;
    }
    const uint8_t data = PIN_BACKLIGHT | PIN_RS;
    uint8_t frame[1U + LCD_COLUMNS * ENCODED_BYTE] = { data };
    uint16_t len = 1U;
    uint8_t col = cursor_column;
    while (col < LCD_COLUMNS && *text != '\0') {
        encode_byte(&frame[len], (uint8_t)*text++, data);
        len += ENCODED_BYTE;
        ++col;
    }
    if (len > 1U && !transmit(frame, len)) {
        return false;
    }
    cursor_column = col;
    return true;
}

bool lcd_print_int(int32_t value)
{
    char text[12];
    char *next = &text[sizeof(text) - 1U];
    *next = '\0';
    uint32_t magnitude = value < 0 ? 0U - (uint32_t)value : (uint32_t)value;
    do {
        *--next = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude != 0U);
    if (value < 0) {
        *--next = '-';
    }
    return lcd_print(next);
}

bool lcd_write_line(uint8_t row, const char *text)
{
    if (!ready || row >= LCD_ROWS || text == NULL) {
        return false;
    }
    const uint8_t data = PIN_BACKLIGHT | PIN_RS;
    uint8_t frame[LINE_FRAME_SIZE] = { PIN_BACKLIGHT };
    encode_byte(&frame[1], cursor_address(row, 0U), PIN_BACKLIGHT);
    /* Set RS before raising Enable for the first data nibble. */
    uint16_t len = 1U + ENCODED_BYTE;
    frame[len++] = data;
    for (uint8_t col = 0U; col < LCD_COLUMNS; ++col) {
        const uint8_t character = *text == '\0' ? (uint8_t)' ' : (uint8_t)*text++;
        encode_byte(&frame[len], character, data);
        len += ENCODED_BYTE;
    }
    if (!transmit(frame, len)) {
        return false;
    }
    cursor_column = LCD_COLUMNS;
    return true;
}
