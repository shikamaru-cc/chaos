#include "debug.h"
#include "sync.h"
#include "stdint.h"
#include "stdnull.h"
#include "thread.h"
#include "interrupt.h"
#include "kernel/list.h"

void sem_init(sem_t* sem, uint8_t value) {
  psem->value = value;
  list_init(&sem->waiters);
}

/* sem_wait
 * The atomic P operation of semaphore. Decrete sem->value when sem->value > 0, 
 * block the thread when sem->value == 0. Disable interrupt to achieve atomic.
 */
void sem_wait(sem_t* sem) {
  enum intr_status old_status = intr_disable();

  /* If the sem->value is zero, then the thread will add itself to the
   * sem->waiters and block itself. When the thread wakes up, check again. */
  while (sem->value == 0) {
    ASSERT(!elem_find(&sem->waiters, &running_thread()->general_tag));
    if (elem_find(&sem->waiters, &running_thread()->general_tag)) {
      PANIC("sem_wait: thread blocked has been in waiters_list\n");
    }
    list_append(&sem->waiters, &running_thread()->general_tag);
    thread_block(TASK_BLOCKED);
  }

  /* sem->value = 1 means the running thread get the lock */
  sem->value--;
  ASSERT(sem->value >= 0);

  intr_set_status(old_status);
}

/* sem_post
 * The atomic V operation of semaphore. Increte sem->value and wake up the first
 * thread in sem->waiters. Disable interrupt to achieve atomic.
 */
void sem_post(sem_t* sem) {
  enum intr_status old_status = intr_disable();

  if (!list_empty(&sem->waiters)) {

    struct task_struct* thread_blocked = elem2entry(struct task_struct,
      general_tag, list_pop(&sem->waiters)); 

    thread_unblock(thread_blocked);
  }

  sem->value++;

  intr_set_status(old_status);
}

void lock_init(lock_t* lock) {
  lock->holder = NULL;
  lock->holder_repeat_nr = 0;
  sem_init(&lock->sem, 1);
}

void lock_acquire(lock_t* lock) {
  if (lock->holder != running_thread()) {
    sem_wait(&lock->sem);
    lock->holder = running_thread();

    ASSERT(lock->holder_repeat_nr == 0);
    lock->holder_repeat_nr = 1;
  } else {
    lock->holder_repeat_nr++;
  }
}

void lock_release(lock_t* lock) {
  ASSERT(lock->holder == running_thread());

  if (lock->holder_repeat_nr > 1) {
    lock->holder_repeat_nr--;
    return;
  }

  ASSERT(lock->holder_repeat_nr == 1);

  lock->holder = NULL;
  lock->holder_repeat_nr = 0;
  sem_post(&lock->sem);
}
