#include "debug.h"
#include "console.h"
#include "init.h"
#include "string.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "kernel/print.h"

void k_thread(void* arg) {
  char* para = arg;
  int i = 0;
  while (1) {
    console_put_str(para);
    console_put_int(i);
    console_put_char(' ');
    i++;
  }
}

int main(void) {
  put_str("\nWelcome to Chaos ..\n");
  init_all();
  thread_start("thread 1", 31, k_thread, "foo ");
  thread_start("thread 2", 8, k_thread, "bar ");

  intr_enable();

  int i = 0;
  while(1) {
    console_put_str("Main ");
    console_put_int(i);
    console_put_char(' ');
    i++;
  }

  return 0;
}
