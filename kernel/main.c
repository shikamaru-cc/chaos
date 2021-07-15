#include "debug.h"
#include "console.h"
#include "init.h"
#include "string.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "process.h"
#include "kernel/print.h"

void k_thread_a(void* arg);
void k_thread_b(void* arg);
void u_proc_a(void);
void u_proc_b(void);

int a = 0, b = 0;

int main(void) {
  put_str("\nWelcome to Chaos ..\n");
  init_all();
  thread_start("thread 1", 31, k_thread_a, "a: ");
  thread_start("thread 2", 31, k_thread_b, "b: ");
  process_execute(u_proc_a, "user_proc_a");
  process_execute(u_proc_b, "user_proc_b");

  intr_enable();

  while(1);

  return 0;
}

void k_thread_a(void* arg) {
  while (1) {
    console_put_str(arg);
    console_put_int(a);
    console_put_char(' ');
  }
}

void k_thread_b(void* arg) {
  while (1) {
    console_put_str(arg);
    console_put_int(b);
    console_put_char(' ');
  }
}


void u_proc_a(void) {
  while (1) {
    a++;
  }
}


void u_proc_b(void) {
  while (1) {
    b++;
  }
}

