#include "syscall.h"
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

uint32_t sys_getpid(void) {
  return running_thread()->pid;
}

pid_t getpid(void) {
  return (pid_t)__syscall0(SYS_getpid);
}

void syscall_init(void) {
  put_str("syscall init start\n");
  syscall_table[SYS_getpid] = sys_getpid;
  put_str("syscall init done\n");
}
