#include "init.h"

#include "console.h"
#include "disk.h"
#include "fs.h"
#include "interrupt.h"
#include "kernel/print.h"
#include "keyboard.h"
#include "memory.h"
#include "syscall.h"
#include "thread.h"
#include "timer.h"
#include "tss.h"

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
