#include "console.h"
#include "init.h"
#include "timer.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "kernel/print.h"

void init_all() {
  put_str("chaos init ..\n");  
  idt_init();
  mem_init();
  thread_init();
  timer_init();
  console_init();
}
