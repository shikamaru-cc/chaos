#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "thread.h"

typedef enum {
  SYS_getpid
} SYSCALL_NUMBER;

pid_t getpid(void);

void syscall_init(void);

#endif
