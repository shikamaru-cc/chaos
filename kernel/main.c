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

typedef struct malloc_test {
  int a;
  int b;
} mtest_t;

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

  mtest_t* mt = (mtest_t*)malloc(sizeof(mtest_t) * 100);

  int a = 0, b = 0;
  mtest_t* mt_iter = mt;
  while (a < 100) {
    mt_iter->a = a;
    mt_iter->b = b;
    printf("user prog a : mt a = %d\n", mt_iter->a);
    printf("user prog a : mt b = %d\n", mt_iter->b);
    mt_iter++;
    a++;
    b--;
  }

  free(mt);
  // page fault should occur here
  printf("user prog a : mt a = %d\n", mt->a);
  printf("user prog a : mt b = %d\n", mt->b);

  while(1);
}

void u_proc_b(void) {
  pid_b = getpid();
  printf("user prog a pid : %d addr : 0x%x\n", pid_b, (uint32_t)u_proc_b);
  printf("%s, I am proc %c, 3 + 256 = %d\n", "Fuck you", 'b', 3+256);

  mtest_t* mt = (mtest_t*)malloc(sizeof(mtest_t));

  int a = 0, b = 0;
  mt->a = a;
  mt->b = b;
  printf("user prog b : mt a = %d\n", mt->a);
  printf("user prog b : mt b = %d\n", mt->b);

  free(mt);
  // page fault maybe not occur here
  printf("user prog b : mt a = %d\n", mt->a);
  printf("user prog b : mt b = %d\n", mt->b);

  while(1);
}

