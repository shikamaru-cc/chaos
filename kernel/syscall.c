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
int32_t __syscall3(SYSCALL_NUMBER n, void* arg0, void* arg1, void* arg2);

uint32_t sys_getpid(void);
pid_t getpid(void);
void* malloc(uint32_t size);
void free(void* va);
int32_t open(const char* pathname, int32_t flags);
int32_t write(int32_t fd, const void* buf, int32_t size);
int32_t read(int32_t fd, void* buf, int32_t size);

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

int32_t __syscall3(SYSCALL_NUMBER n, void* arg0, void* arg1, void* arg2) {
  int32_t __ret;
  asm volatile("int $0x80"
               : "=a"(__ret)
               : "a"(n), "b"(arg0), "c"(arg1), "d"(arg2));
  return __ret;
}

uint32_t sys_getpid(void) { return running_thread()->pid; }

pid_t getpid(void) { return (pid_t)__syscall0(SYS_GETPID); }

void* malloc(uint32_t size) { return (void*)__syscall1(SYS_MALLOC, size); }

void free(void* va) { __syscall1(SYS_FREE, va); }

int32_t open(const char* pathname, int32_t flags) {
  return __syscall2(SYS_OPEN, pathname, flags);
}

int32_t write(int32_t fd, const void* buf, int32_t size) {
  return __syscall3(SYS_WRITE, fd, buf, size);
}

int32_t read(int32_t fd, void* buf, int32_t size) {
  return __syscall3(SYS_READ, fd, buf, size);
}

void syscall_init(void) {
  put_str("syscall init start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  syscall_table[SYS_MALLOC] = sys_malloc;
  syscall_table[SYS_FREE] = sys_free;
  syscall_table[SYS_OPEN] = sys_open;
  syscall_table[SYS_WRITE] = sys_write;
  syscall_table[SYS_READ] = sys_read;
  put_str("syscall init done\n");
}
