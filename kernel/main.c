#include "debug.h"
#include "console.h"
#include "init.h"
#include "string.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "kernel/print.h"

/* TODO: for test, remove it later */
#include "ioqueue.h"
#include "keyboard.h"

void k_thread(void* arg) {
  char* para = arg;
  char ch;
  while (1) {
    ch = ioq_getchar(&kbd_buf);
    console_put_str(para);
    console_put_char(ch);
    console_put_char(' ');
  }
}

int main(void) {
  put_str("\nWelcome to Chaos ..\n");
  init_all();
  thread_start("thread 1", 31, k_thread, "A_");
  thread_start("thread 2", 31, k_thread, "B_");

  intr_enable();

  while(1);

  return 0;
}
