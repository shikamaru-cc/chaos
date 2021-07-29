#ifndef __DEVICE_DISK_H
#define __DEVICE_DISK_H

#include "stdint.h"
#include "sync.h"
#include "thread.h"

enum disk_type {
  DISK_MASTER,
  DISK_SLAVE
};

struct ide_channel {
  uint16_t port_base; // Control port base
  uint8_t irq_no;
  lock_t lock;
  struct task_struct* user_thread; // The thread using this channel
};

#endif
