#include "ioqueue.h"

#include "debug.h"
#include "stdbool.h"
#include "string.h"
#include "sync.h"

bool ioq_full(ioqueue_t* ioq) { return ioq->qcount == IOQUEUE_BUFSIZE; }

bool ioq_empty(ioqueue_t* ioq) { return ioq->qcount == 0; }

void ioq_init(ioqueue_t* ioq) {
  lock_init(&ioq->lock);
  cond_init(&ioq->cond_sender, &ioq->lock);
  cond_init(&ioq->cond_recver, &ioq->lock);
  ioq->sendx = 0;
  ioq->recvx = 0;
  ioq->qcount = 0;
  memset(ioq->buf, 0, IOQUEUE_BUFSIZE);
}

void ioq_putchar(ioqueue_t* ioq, char ch) {
  lock_acquire(&ioq->lock);

  while (ioq_full(ioq)) {
    cond_wait(&ioq->cond_sender);
  }

  ioq->qcount++;
  ASSERT(ioq->qcount <= IOQUEUE_BUFSIZE);
  ioq->buf[ioq->sendx] = ch;
  ioq->sendx = (ioq->sendx + 1) % IOQUEUE_BUFSIZE;

  cond_signal(&ioq->cond_recver);

  lock_release(&ioq->lock);
}

char ioq_getchar(ioqueue_t* ioq) {
  lock_acquire(&ioq->lock);

  while (ioq_empty(ioq)) {
    cond_wait(&ioq->cond_recver);
  }

  ioq->qcount--;
  ASSERT(ioq->qcount >= 0);
  char ch = ioq->buf[ioq->recvx];
  ioq->recvx = (ioq->recvx + 1) % IOQUEUE_BUFSIZE;

  cond_signal(&ioq->cond_sender);

  lock_release(&ioq->lock);

  return ch;
}
