#include "init.h"
#include "print.h"
#include "interrupt.h"

void init_all(void) {
  put_str("chaos init ..\n");  
  idt_init();
}
