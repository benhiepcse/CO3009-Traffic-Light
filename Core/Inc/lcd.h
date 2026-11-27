#ifndef LCD_H
#define LCD_H

#include <stdbool.h>
#include <stdint.h>

/* Call once during startup, after I2C1 has been initialized. */
bool lcd_init(void);

/* Row 0 is the first line. Short strings are padded with spaces. */
bool lcd_write_line(uint8_t row, const char *text);

#endif
