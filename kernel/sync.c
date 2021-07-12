#include "debug.h"
#include "sync.h"
#include "stdint.h"
#include "stdnull.h"
#include "thread.h"
#include "interrupt.h"
#include "kernel/list.h"

/* =========================== Semaphore implement ========================== */

void sem_init(sem_t* sem, uint8_t value) {
  sem->value = value;
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

  struct task_struct* thread_blocked;

  if (!list_empty(&sem->waiters)) {

    thread_blocked = elem2entry(
                      struct task_struct,
                      general_tag,
                      list_pop(&sem->waiters)
                     );

    thread_unblock(thread_blocked);

  }

  sem->value++;

  intr_set_status(old_status);
}

/* ============================= Mutex implement ============================ */

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

/* ===================== Condition variable implement ======================= */

void cond_init(cond_t* cond, lock_t* lock) {
  cond->lock = lock;
  list_init(&cond->waitq);
}

void cond_wait(cond_t* cond) {
  list_append(&cond->waitq, &running_thread()->general_tag);
  lock_release(cond->lock);
  thread_block(TASK_BLOCKED);
  lock_acquire(cond->lock);
}

void cond_signal(cond_t* cond) {
  struct task_struct* thread_blocked;

  if (!list_empty(&cond->waitq)) {

    thread_blocked = elem2entry(
                      struct task_struct,
                      general_tag,
                      list_pop(&cond->waitq)
                     );

    thread_unblock(thread_blocked);

  }
}
