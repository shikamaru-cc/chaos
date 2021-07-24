#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "stdint.h"
#include "thread.h"

typedef enum {
  SYS_GETPID,
  SYS_WRITE,
  SYS_MALLOC
} SYSCALL_NUMBER;

pid_t getpid(void);
uint32_t write(char* str);
void* malloc(uint32_t size);

void syscall_init(void);

#endif
