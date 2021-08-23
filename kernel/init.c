#include "console.h"
#include "disk.h"
#include "init.h"
#include "timer.h"
#include "interrupt.h"
#include "memory.h"
#include "syscall.h"
#include "tss.h"
#include "thread.h"
#include "fs.h"
#include "keyboard.h"
#include "kernel/print.h"

void init_all() {
  put_str("chaos init ..\n");  
  idt_init();
  mem_init();
  thread_init();
  tss_init();
  timer_init();
  console_init();
  keyboard_init();
  syscall_init();
  disk_init();
  fs_init();
}
