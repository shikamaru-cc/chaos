#ifndef __TIMER_H
#define __TIMER_H
#include "stdint.h"
static void timer_set_frequence(uint8_t counter_port, 
                                uint8_t counter_no,
                                uint8_t rwl,
                                uint8_t counter_mode,
                                uint16_t counter_value);
void timer_init(void);
#endif
