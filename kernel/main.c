#include "init.h"
#include "debug.h"
#include "string.h"
#include "interrupt.h"
#include "memory.h"
#include "kernel/print.h"

void main_thread(void* arg) {
  put_str((char*)arg);
  while(1);
}

int main(void) {
  put_str("\nWelcome to Chaos ..\n");
  init_all();
  intr_set_status(INTR_OFF);

  void* addr = get_kernel_pages(3);
  put_str("\n get_kernel_page start vaddr : ");
  put_int((uint32_t)addr);  
  put_str("\n");

  struct task_status* thread = thread_start("hello", 1, main_thread, "world");  
  return 0;
}
