#include "init.h"
#include "timer.h"
#include "interrupt.h"
#include "kernel/print.h"

void init_all(void) {
  put_str("chaos init ..\n");  
  idt_init();
  timer_init();
}
