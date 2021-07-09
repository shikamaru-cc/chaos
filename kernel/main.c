#include "init.h"
#include "debug.h"
#include "string.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "kernel/print.h"

void k_thread(void* arg) {
  char* para = arg;
  int i = 0;
  while (1) {
    intr_disable();
    put_str(para);
    put_int(i);
    intr_enable();
    i++;
  }
}

int main(void) {
  put_str("\nWelcome to Chaos ..\n");
  init_all();
  thread_start("thread 1", 50, k_thread, "foo ");
  thread_start("thread 2", 8, k_thread, "bar ");

  intr_enable();

  int i = 0;
  while(1) {
    intr_disable();
    put_str("Main ");
    put_int(i);
    intr_enable();
    i++;
  }

  return 0;
}
