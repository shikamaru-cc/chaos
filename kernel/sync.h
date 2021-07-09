#ifndef __THREAD_SYNC_H
#define __THREAD_SYNC_H

#include "stdint.h"
#include "thread.h"
#include "kernel/list.h"

typedef struct {
  uint8_t value;
  struct list waiters;
} sem_t;

typedef struct {
  struct task_struct* holder; /* lock holder*/
  sem_t sem;
  uint32_t holder_repeat_nr;
} lock_t;

void lock_init(lock_t* lock);
void lock_acquire(lock_t* lock);
void lock_release(lock_t* lock);
#endif
