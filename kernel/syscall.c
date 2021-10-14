#include "syscall.h"

#include "console.h"
#include "fs.h"
#include "kernel/print.h"
#include "stdint.h"
#include "string.h"
#include "thread.h"

int32_t __syscall0(SYSCALL_NUMBER n);
int32_t __syscall1(SYSCALL_NUMBER n, void* arg0);
int32_t __syscall2(SYSCALL_NUMBER n, void* arg0, void* arg1);
uint32_t sys_getpid(void);
pid_t getpid(void);
uint32_t sys_write(char* str);
uint32_t write(char* str);
void* malloc(uint32_t size);
void free(void* va);
int32_t open(const char* pathname, int32_t flags);
void syscall_init(void);

int32_t __syscall0(SYSCALL_NUMBER n) {
  int32_t __ret;
  asm volatile("int $0x80" : "=a"(__ret) : "a"(n));
  return __ret;
}

int32_t __syscall1(SYSCALL_NUMBER n, void* arg0) {
  int32_t __ret;
  asm volatile("int $0x80" : "=a"(__ret) : "a"(n), "b"(arg0));
  return __ret;
}

int32_t __syscall2(SYSCALL_NUMBER n, void* arg0, void* arg1) {
  int32_t __ret;
  asm volatile("int $0x80" : "=a"(__ret) : "a"(n), "b"(arg0), "c"(arg1));
  return __ret;
}

uint32_t sys_getpid(void) { return running_thread()->pid; }

pid_t getpid(void) { return (pid_t)__syscall0(SYS_GETPID); }

uint32_t sys_write(char* str) {
  console_put_str(str);
  return strlen(str);
}

uint32_t write(char* str) { return __syscall1(SYS_WRITE, str); }

void* malloc(uint32_t size) { return (void*)__syscall1(SYS_MALLOC, size); }

void free(void* va) { __syscall1(SYS_FREE, va); }

int32_t open(const char* pathname, int32_t flags) {
  return __syscall2(SYS_OPEN, pathname, flags);
}

void syscall_init(void) {
  put_str("syscall init start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  syscall_table[SYS_WRITE] = sys_write;
  syscall_table[SYS_MALLOC] = sys_malloc;
  syscall_table[SYS_FREE] = sys_free;
  syscall_table[SYS_OPEN] = sys_open;
  put_str("syscall init done\n");
}
