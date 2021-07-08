#include "init.h"
#include "debug.h"
#include "string.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "kernel/print.h"

void k_thread(void* arg) {
  char* para = arg;
  while (1) {
    put_str(para);
  }
}

int main(void) {
  put_str("\nWelcome to Chaos ..\n");
  init_all();
  thread_start("thread 1", 31, k_thread, "foo\n");
  thread_start("thread 2", 8, k_thread, "bar\n");

  intr_enable();

  while(1) {
    put_str("Main\n");
  }

  return 0;
}
