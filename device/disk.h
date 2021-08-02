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

void disk_init(void);

// FIXME: remove follow to private

#define IDE_MAX_IDE_CHANNLES 2
struct ide_channel ide_channels[IDE_MAX_IDE_CHANNLES];

void ide_channel_read(struct ide_channel* ide, uint32_t lba, \
  uint32_t sec_cnt, enum disk_type dt, void* buf);

void ide_channel_write(struct ide_channel* ide, uint32_t lba, \
  uint32_t sec_cnt, enum disk_type dt, void* buf);

#endif
