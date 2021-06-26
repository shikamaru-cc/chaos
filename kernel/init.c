#include "init.h"
#include "timer.h"
#include "interrupt.h"
#include "memory.h"
#include "kernel/print.h"

void init_all() {
  put_str("chaos init ..\n");  
  idt_init();
  timer_init();
  mem_init();
}
