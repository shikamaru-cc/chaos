#include "debug.h"
#include "console.h"
#include "init.h"
#include "string.h"
#include "syscall.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "process.h"
#include "stdio.h"
#include "kernel/print.h"

void k_thread_a(void* arg);
void k_thread_b(void* arg);
void u_proc_a(void);
void u_proc_b(void);

pid_t pid_a, pid_b;

int main(void) {
  put_str("\nWelcome to Chaos ..\n");
  init_all();

  process_execute(u_proc_a, "user_proc_a");
  process_execute(u_proc_b, "user_proc_b");

  thread_start("thread 1", 31, k_thread_a, "I am thread a\n");
  thread_start("thread 2", 31, k_thread_b, "I am thread b\n");

  intr_enable();

  while(1);

  return 0;
}

void k_thread_a(void* arg) {
  pid_t pid = getpid();
  console_put_str("pid ");
  console_put_int(pid);
  console_put_str(" : ");
  console_put_str(arg);
  console_put_str("pid ");
  console_put_int(pid_a);
  console_put_str(" : ");
  console_put_str("user proc a\n");
  while(1);
}

void k_thread_b(void* arg) {
  pid_t pid = getpid();
  console_put_str("pid ");
  console_put_int(pid);
  console_put_str(" : ");
  console_put_str(arg);
  console_put_str("pid ");
  console_put_int(pid_b);
  console_put_str(" : ");
  console_put_str("user proc b\n");
  while(1);
}

void u_proc_a(void) {
  pid_a = getpid();
  printf("user prog a pid : %d addr : 0x%x\n", pid_a, (uint32_t)u_proc_a);
  char buf[1024];
  sprintf(buf, "%s, I am proc %c, 1 - 100 = %d\n", "Hello", 'a', 1-100);
  printf("%s", buf);
  while(1);
}

void u_proc_b(void) {
  pid_b = getpid();
  printf("user prog a pid : %d addr : 0x%x\n", pid_b, (uint32_t)u_proc_b);
  printf("%s, I am proc %c, 3 + 256 = %d\n", "Fuck you", 'b', 3+256);
  while(1);
}

