#include "console.h"

#include "kernel/print.h"
#include "stdint.h"
#include "sync.h"
#include "thread.h"

static lock_t console_lock;

void console_init() { lock_init(&console_lock); }

void console_put_str(char* str) {
  lock_acquire(&console_lock);
  put_str(str);
  lock_release(&console_lock);
}

void console_put_char(uint8_t ch) {
  lock_acquire(&console_lock);
  put_char(ch);
  lock_release(&console_lock);
}

void console_put_int(uint32_t num) {
  lock_acquire(&console_lock);
  put_int(num);
  lock_release(&console_lock);
}
