#include "debug.h"
#include "spinlock.h"
#include "interrupt.h"

void spinlock_init(spinlock_t* lock) {
  *lock = 0;
}

void spinlock_acquire(spinlock_t* lock) {
  enum intr_status old_status;
  while(1) {
    old_status = intr_disable();
    if (*lock == 0) {
      break;
    }
    intr_set_status(old_status);
  }
  *lock += 1;
  intr_set_status(old_status);
  return;
}

void spinlock_release(spinlock_t* lock) {
  enum intr_status old_status = intr_disable();
  ASSERT(*lock == 1);
  *lock -= 1;
  intr_set_status(old_status);
}
