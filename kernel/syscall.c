#include "syscall.h"
#include "console.h"
#include "thread.h"
#include "kernel/print.h"
#include "stdint.h"

#define syscall_nr 32

typedef void* syscall;

syscall syscall_table[syscall_nr];

int32_t __syscall0(SYSCALL_NUMBER n) {
  int32_t __ret;
  asm volatile(
    "int $0x80"
    : "=a" (__ret)
    : "a" (n)
  );
  return __ret;
}

int32_t __syscall1(SYSCALL_NUMBER n, void* arg0) {
  int32_t __ret;
  asm volatile(
    "int $0x80"
    : "=a" (__ret)
    : "a" (n), "b" (arg0)
  );
  return __ret;
}

uint32_t sys_getpid(void) {
  return running_thread()->pid;
}

pid_t getpid(void) {
  return (pid_t)__syscall0(SYS_GETPID);
}

uint32_t sys_write(char* str) {
  console_put_str(str);
  return strlen(str);
}

uint32_t write(char* str) {
  return __syscall1(SYS_WRITE, str);
}

void* malloc(uint32_t size) {
  return (void*)__syscall1(SYS_MALLOC, size);
}

void syscall_init(void) {
  put_str("syscall init start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  syscall_table[SYS_WRITE] = sys_write;
  syscall_table[SYS_MALLOC] = sys_malloc;
  put_str("syscall init done\n");
}
