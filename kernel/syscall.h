#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "stdint.h"
#include "thread.h"

typedef enum {
  SYS_GETPID,
  SYS_WRITE
} SYSCALL_NUMBER;

pid_t getpid(void);
uint32_t write(char* str);

void syscall_init(void);

#endif
