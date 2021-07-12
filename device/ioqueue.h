#ifndef __DEVICE_IOQUEUE_H
#define __DEVICE_IOQUEUE_H

#include "stdint.h"
#include "sync.h"

#define IOQUEUE_BUFSIZE 256

typedef struct {
  lock_t lock; /* mutex for condition variable */
  cond_t cond_sender; /* condition variable for sender */
  cond_t cond_recver; /* condition variable for receiver */
  uint32_t sendx; /* next index for producing */
  uint32_t recvx; /* next index for consuming */
  uint32_t qcount; /* number of elements waiting to be consumed */
  char buf[IOQUEUE_BUFSIZE]; /* data buffer */
} ioqueue_t;

void ioq_init(ioqueue_t* ioq);
void ioq_putchar(ioqueue_t* ioq, char ch);
char ioq_getchar(ioqueue_t* ioq);

#endif
