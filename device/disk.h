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
  sem_t sem;
};

struct disk {
  struct ide_channel* ide;
  enum disk_type dt;
  char seq[21];
  char module[41];
  uint32_t sec_total;
};

// We only support 4 disk now
#define DISK_MAX_CNT 4
struct disk disks[DISK_MAX_CNT];

void disk_read(struct disk* hd, void* buf, uint32_t lba, uint32_t sec_cnt);

void disk_write(struct disk* hd, void* buf, uint32_t lba, uint32_t sec_cnt);

void disk_init(void);

#endif
