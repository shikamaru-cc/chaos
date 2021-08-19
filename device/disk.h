#ifndef __DEVICE_DISK_H
#define __DEVICE_DISK_H

#include "stdint.h"
#include "sync.h"
#include "thread.h"
#include "kernel/list.h"

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
  char name[8];
  char seq[21];
  char module[41];
  uint32_t sec_total;
  uint32_t part_cnt;
};

// We only support 4 disk now
#define DISK_MAX_CNT 4
struct disk disks[DISK_MAX_CNT];

void disk_read(struct disk* hd, void* buf, uint32_t lba, uint32_t sec_cnt);

void disk_write(struct disk* hd, void* buf, uint32_t lba, uint32_t sec_cnt);

void disk_init(void);

struct partition {
  struct disk* hd;
  uint32_t lba_start;
  uint32_t sec_cnt;
  uint8_t fs_type;
  struct list_elem tag;
  char name[8];
};

struct list disk_partitions;

#endif
