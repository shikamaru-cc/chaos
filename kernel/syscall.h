#ifndef __SYSCALL_H
#define __SYSCALL_H

#include "stdint.h"
#include "thread.h"

typedef enum {
  SYS_GETPID,
  SYS_MALLOC,
  SYS_FREE,
  SYS_OPEN,
  SYS_CLOSE,
  SYS_WRITE,
  SYS_READ,
  SYS_LSEEK,
  SYS_UNLINK,
} SYSCALL_NUMBER;

typedef void* syscall;

#define syscall_nr 32
syscall syscall_table[syscall_nr];

pid_t getpid(void);
void* malloc(uint32_t size);
void free(void* va);
int32_t open(const char* pathname, int32_t flags);
int32_t close(int32_t fd);
int32_t write(int32_t fd, const void* buf, int32_t size);
int32_t read(int32_t fd, void* buf, int32_t size);
int32_t lseek(int32_t fd, int32_t offset, int32_t whence);
int32_t unlink(const char* pathname);

void syscall_init(void);

#endif
