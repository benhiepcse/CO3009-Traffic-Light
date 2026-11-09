#ifndef LCD_H
#define LCD_H

#include <stdbool.h>
#include <stdint.h>

bool lcd_init(void);
bool lcd_write_line(uint8_t row, const char *text);

#endif
