#include "lcd.h"

#include <stddef.h>

#ifdef LCD_HOST_TEST
extern bool lcd_test_write(uint16_t address, const uint8_t *data, uint16_t len,
                           uint32_t timeout_ms);
extern bool lcd_test_probe(uint16_t address);
extern bool lcd_test_bus_busy(void);
extern uint32_t lcd_test_now_ms(void);
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

#ifdef CO3009_LCD_ADDRESS
_Static_assert((CO3009_LCD_ADDRESS >= 0x20U && CO3009_LCD_ADDRESS <= 0x27U) ||
               (CO3009_LCD_ADDRESS >= 0x39U && CO3009_LCD_ADDRESS <= 0x3FU),
               "Select a PCF8574(A) address distinct from DHT20 (0x38)");
static const uint8_t candidates[] = { CO3009_LCD_ADDRESS };
#else
static const uint8_t candidates[] = {
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};
#endif

static enum { OFFLINE, PROBING, INITIALIZING, READY } state;
static uint8_t candidate_index, address_7bit, init_step;
static uint32_t last_step_ms, delay_ms;
static uint8_t cursor_column;

static uint32_t now_ms(void)
{
#ifdef LCD_HOST_TEST
    return lcd_test_now_ms();
#else
    return HAL_GetTick();
#endif
}

static void schedule_step(uint32_t delay)
{
    last_step_ms = now_ms();
    delay_ms = delay;
}

static void retry_later(void)
{
    state = OFFLINE;
    schedule_step(500U);
}

static bool bus_busy(void)
{
#ifdef LCD_HOST_TEST
    return lcd_test_bus_busy();
#else
    return __HAL_I2C_GET_FLAG(&hi2c1, I2C_FLAG_BUSY);
#endif
}

static bool bus_idle(void)
{
    /* F1 HAL can return before its own STOP has cleared BUSY on the wire. */
    const uint32_t start = now_ms();
    while (bus_busy()) {
        if (now_ms() - start >= 2U) {
            return false;
        }
    }
    return true;
}

static bool probe(uint8_t address)
{
#ifdef LCD_HOST_TEST
    return lcd_test_probe((uint16_t)(address << 1U));
#else
    return HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(address << 1U), 1U, 2U) == HAL_OK;
#endif
}

static bool transmit(uint8_t *data, uint16_t len)
{
    const uint16_t address = (uint16_t)(address_7bit << 1U);
#ifdef LCD_HOST_TEST
    const bool ok = bus_idle() && lcd_test_write(address, data, len, I2C_TIMEOUT_MS);
#else
    const bool ok = bus_idle() &&
        HAL_I2C_Master_Transmit(&hi2c1, address, data, len, I2C_TIMEOUT_MS) == HAL_OK;
#endif
    if (!ok) {
        /* A lost device or interrupted nibble requires a full 4-bit resync. */
        retry_later();
    }
    return ok;
}

static void wait_ms(uint32_t ms)
{
#ifdef LCD_HOST_TEST
    lcd_test_wait_ms(ms);
#else
    HAL_Delay(ms);
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

static void start_scan(void)
{
    state = PROBING;
    candidate_index = 0U;
    address_7bit = 0U;
    init_step = 0U;
}

/* One address probe or one LCD transfer per call. Delays run between calls. */
static void advance_init(void)
{
    if (state == OFFLINE) {
        start_scan();
    }
    if (state == PROBING) {
        if (!bus_idle()) {
            retry_later();
            return;
        }
        const uint8_t addr = candidates[candidate_index++];
        if (probe(addr)) {
            if (address_7bit != 0U) {
                retry_later();  /* Ambiguous bus: do not write to either device. */
                return;
            }
            address_7bit = addr;
        }
        if (candidate_index == sizeof candidates) {
            if (address_7bit == 0U) {
                retry_later();
                return;
            }
            state = INITIALIZING;
        }
        schedule_step(2U);
        return;
    }

    static const struct {
        uint8_t value;
        uint8_t delay_after_ms;
    } steps[] = {
        { PIN_BACKLIGHT, 3U },                   /* Idle, E low */
        { 0x30U, 6U }, { 0x30U, 2U }, { 0x30U, 2U }, { 0x20U, 2U },
        { 0x28U, 2U }, { 0x08U, 2U }, { CLEAR_DISPLAY, 3U },
        { 0x06U, 2U }, { 0x0CU, 2U }
    };
    uint8_t value = steps[init_step].value;
    bool ok;
    if (init_step == 0U) {
        ok = transmit(&value, 1U);
    } else if (init_step <= 4U) {
        ok = init_nibble(value);
    } else {
        ok = command(value);
    }
    if (ok) {
        schedule_step(steps[init_step].delay_after_ms);
        if (++init_step == sizeof steps / sizeof steps[0]) {
            cursor_column = 0U;
            state = READY;
        }
    }
}

bool lcd_init(void)
{
    start_scan();
    wait_ms(50U);
    do {
        advance_init();
        if (state == OFFLINE) {
            return false;
        }
        wait_ms(delay_ms);
    } while (state != READY);
    return true;
}

bool lcd_poll_ready(void)
{
    if (state == READY) {
        return true;
    }
    if (now_ms() - last_step_ms >= delay_ms) {
        advance_init();
    }
    return false;  /* Drawing resumes on the call after initialization. */
}

bool lcd_clear(void)
{
    if (state != READY || !command(CLEAR_DISPLAY)) {
        return false;
    }
    wait_ms(3U);
    cursor_column = 0U;
    return true;
}

bool lcd_set_cursor(uint8_t row, uint8_t column)
{
    if (state != READY || row >= LCD_ROWS || column >= LCD_COLUMNS ||
        !command(cursor_address(row, column))) {
        return false;
    }
    cursor_column = column;
    return true;
}

bool lcd_print(const char *text)
{
    if (state != READY || text == NULL) {
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
    if (state != READY || row >= LCD_ROWS || text == NULL) {
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
