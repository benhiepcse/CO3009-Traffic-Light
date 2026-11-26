#ifndef LCD_H
#define LCD_H

#include <stdbool.h>
#include <stdint.h>

/* Provisional address. Override with the result of the CO3009-04 I2C scan. */
#ifndef CO3009_LCD_ADDRESS
#define CO3009_LCD_ADDRESS 0x27U
#endif

#define LCD_ROWS 2U
#define LCD_COLUMNS 16U

/* Call at startup in the main context after I2C1 initialization. */
bool lcd_init(void);
bool lcd_clear(void);
bool lcd_set_cursor(uint8_t row, uint8_t column);
bool lcd_print(const char *text);
bool lcd_print_int(int32_t value);
/* Row is zero-based. The text is clipped or padded to LCD_COLUMNS. */
bool lcd_write_line(uint8_t row, const char *text);

#endif
