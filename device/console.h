#ifndef __CONSOLE_H
#define __CONSOLE_H
#include "stdint.h"
void console_init(void);
void console_put_str(char* str);
void console_put_char(uint8_t ch);
void console_put_int(uint32_t num);
#endif
