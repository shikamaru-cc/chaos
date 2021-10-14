#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "stdint.h"
#include "thread.h"

typedef enum {
  SYS_GETPID,
  SYS_WRITE,
  SYS_MALLOC,
  SYS_FREE,
  SYS_OPEN
} SYSCALL_NUMBER;

typedef void* syscall;

#define syscall_nr 32
syscall syscall_table[syscall_nr];

pid_t getpid(void);
uint32_t write(char* str);
void* malloc(uint32_t size);
void free(void* va);
int32_t open(const char* pathname, int32_t flags);

void syscall_init(void);

#endif
