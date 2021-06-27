#include "init.h"
#include "debug.h"
#include "string.h"
#include "interrupt.h"
#include "memory.h"
#include "kernel/print.h"

int main(void) {
  put_str("\nWelcome to Chaos ..\n");
  init_all();
  intr_set_status(INTR_OFF);

  void* addr = get_kernel_pages(3);
  put_str("\n get_kernel_page start vaddr : ");
  put_int((uint32_t)addr);  
  put_str("\n");

  while(1);
  return 0;
}
