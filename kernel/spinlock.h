#ifndef __SPINLOCK_H
#define __SPINLOCK_H

typedef int spinlock_t;

void spinlock_init(spinlock_t* lock);
void spinlock_acquire(spinlock_t* lock);
void spinlock_release(spinlock_t* lock);

#endif
