#include "init.h"
#include "timer.h"
#include "print.h"
#include "interrupt.h"

void init_all(void) {
  put_str("chaos init ..\n");  
  idt_init();
  timer_init();
}
