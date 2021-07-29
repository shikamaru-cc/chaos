#ifndef __DEVICE_TIMER_H
#define __DEVICE_TIMER_H
#include "stdint.h"
void sys_milisleep(uint32_t miliseconds);
void sys_sleep(uint32_t seconds);
void timer_init(void);
#endif
