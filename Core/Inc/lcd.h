#ifndef LCD_H
#define LCD_H

#include <stdbool.h>
#include <stdint.h>

/* Define CO3009_LCD_ADDRESS to pin the 7-bit backpack address on a shared bus.
 * Otherwise detect the single PCF8574(A) candidate on the LCD/DHT20 bus. */

#define LCD_ROWS 2U
#define LCD_COLUMNS 16U

/* Call at startup in the main context after I2C1 initialization. */
bool lcd_init(void);
/* Main-context display task: advance recovery one step, true when ready to draw. */
bool lcd_poll_ready(void);
bool lcd_clear(void);
bool lcd_set_cursor(uint8_t row, uint8_t column);
bool lcd_print(const char *text);
bool lcd_print_int(int32_t value);
/* Row is zero-based. The text is clipped or padded to LCD_COLUMNS. */
bool lcd_write_line(uint8_t row, const char *text);

#endif
