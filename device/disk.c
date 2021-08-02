#include "debug.h"
#include "disk.h"
#include "kernel/print.h"
#include "kernel/io.h"

// --------------------------- Struct ide_channel --------------------------- //
// defined in disk.h

// Macros

#define ide_channel_data(ide)     (ide->port_base + 0)
#define ide_channel_error(ide)    (ide->port_base + 1)
#define ide_channel_feature(ide)  (ide->port_base + 1)
#define ide_channel_sec(ide)      (ide->port_base + 2)
#define ide_channel_lba1(ide)     (ide->port_base + 3)
#define ide_channel_lba2(ide)     (ide->port_base + 4)
#define ide_channel_lba3(ide)     (ide->port_base + 5)
#define ide_channel_dev(ide)      (ide->port_base + 6)
#define ide_channel_status(ide)   (ide->port_base + 7)
#define ide_channel_cmd(ide)      (ide->port_base + 7)

#define IDE_DEV_MBS1    (1 << 7)
#define IDE_DEV_MOD_LBA (1 << 6)
#define IDE_DEV_MBS2    (1 << 5)
#define IDE_DEV_MASTER  (0 << 4)
#define IDE_DEV_SLAVE   (1 << 4)

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_IDEN  0xEC

#define IDE_STATUS_BSY (1 << 7)
#define IDE_STATUS_DRQ (1 << 3)

// Private variables

// FIXME: We only support 2 ide channels yet

// Private methods

// FIXME: change follow methods to private

void ide_channel_setup(struct ide_channel* ide, uint32_t lba, \
  uint8_t sec_cnt, enum disk_type dt);

void ide_channel_read(struct ide_channel* ide, uint32_t lba, \
  uint32_t sec_cnt, enum disk_type dt, void* buf);

void ide_channel_write(struct ide_channel* ide, uint32_t lba, \
  uint32_t sec_cnt, enum disk_type dt, void* buf);

static void ide_channel_init(uint32_t ide_channel_cnt);

static void intr_disk_handler(uint8_t irq_no);

// ------------------------------ Implementation ---------------------------- //

// ide_channel_setup
// Setup ide channel's sector count, lba and device register before sending
// command.
void ide_channel_setup(struct ide_channel* ide, uint32_t lba, \
  uint8_t sec_cnt, enum disk_type dt) {
  // Write sector count
  outb(ide_channel_sec(ide), sec_cnt);

  // Write LBA 0~7
  outb(ide_channel_lba1(ide), lba);
  // Write LBA 8~15
  outb(ide_channel_lba2(ide), lba >> 8);
  // Write LBA 16~23
  outb(ide_channel_lba3(ide), lba >> 16);

  // Write Device
  uint8_t device = IDE_DEV_MBS1 | IDE_DEV_MOD_LBA | IDE_DEV_MBS2;
  device |= (lba >> 24);
  if (dt == DISK_MASTER) {
    device |= IDE_DEV_MASTER;
  } else {
    device |= IDE_DEV_SLAVE;
  }
  outb(ide_channel_dev(ide), device);

  return;
}

void ide_channel_read(struct ide_channel* ide, uint32_t lba, \
  uint32_t sec_cnt, enum disk_type dt, void* buf) {
  // Two disk share one channel, we need lock
  lock_acquire(&ide->lock);

  uint8_t nsec = 0;
  while (sec_cnt != 0) {
    // We can only write 255 sectors each time, since the sec_cnt reg is 8 bit
    if (sec_cnt > 255) {
      nsec = 255;
      sec_cnt -= 255;
    } else {
      nsec = sec_cnt
      sec_cnt = 0;
    }

    ide_channel_setup(ide, lba, nsec, dt);

    // Write command read
    outb(ide_channel_cmd(ide), IDE_CMD_READ);

    // Wait disk interrupt to wake me up
    ide->user_thread = running_thread();
    thread_block(TASK_BLOCKED);

    uint8_t status = inb(ide_channel_status(ide));
    if (status & IDE_STATUS_BSY) {
      PANIC("Why you are busy ???");
    } else if (!(status & IDE_STATUS_DRQ)) {
      PANIC("Why you are not ready ???");
    }

    uint32_t wordcount = nsec * 512 / 2;
    insw(ide_channel_data(ide), buf, wordcount);

    // Update lba and data ptr for next loop
    lba += nsec;
    buf += wordcount * 2;
  }

  lock_release(&ide->lock);
  return;
}

void ide_channel_write(struct ide_channel* ide, uint32_t lba, \
  uint32_t sec_cnt, enum disk_type dt, void* buf) {
  // Two disk share one channel, we need lock
  lock_acquire(&ide->lock);

  uint8_t nsec = 0;
  while (sec_cnt != 0) {
    // We can only write 255 sectors each time, since the sec_cnt reg is 8 bit
    if (sec_cnt > 255) {
      nsec = 255;
      sec_cnt -= 255;
    } else {
      nsec = sec_cnt
      sec_cnt = 0;
    }

    ide_channel_setup(ide, lba, nsec, dt);

    // Write command write
    outb(ide_channel_cmd(ide), IDE_CMD_WRITE);

    // Wait for disk ready
    ide->user_thread = running_thread();
    thread_block(TASK_BLOCKED);

    uint8_t status = inb(ide_channel_status(ide));
    if (status & IDE_STATUS_BSY) {
      PANIC("Why you are busy ???");
    } else if (!(status & IDE_STATUS_DRQ)) {
      PANIC("Why you are not ready ???");
    }

    uint32_t wordcount = nsec * 512 / 2;
    outsw(ide_channel_data(ide), buf, wordcount);

    // Wait for finishing writing
    ide->user_thread = running_thread();
    thread_block(TASK_BLOCKED);

    // Update lba and data ptr for next loop
    lba += nsec;
    buf += wordcount;
  }

  lock_release(&ide->lock);
  return;
}

static void ide_channel_init(uint32_t ide_channel_cnt) {
  ASSERT(ide_channel_cnt <= IDE_MAX_IDE_CHANNLES);
  struct ide_channel* ide;

  if (ide_channel_cnt > 0) {
    ide = &ide_channels[0];
    ide->port_base = 0x1f0;
    ide->irq_no = 0x2e;
    lock_init(&ide->lock);
    ide->user_thread = NULL;
  }

  if (ide_channel_cnt > 1) {
    ide = &ide_channels[1];
    ide->port_base = 0x170;
    ide->irq_no = 0x2f;
    lock_init(&ide->lock);
    ide->user_thread = NULL;
  }

  return;
}

static void intr_disk_handler(uint8_t irq_no) {
  ASSERT(irq_no == 0x2e || irq_no == 0x2f);
  uint8_t ch_no = irq_no - 0x2e;
  struct ide_channel* ide = ide_channels[ch_no];
  ASSERT(ide->irq_no == irq_no);
  if (ide->user_thread != NULL) {
    thread_unblock(ide->user_thread);
    ide->user_thread = NULL;
  }
  // inform disk interrupt has been handled;
  inb(ide_channel_status(ide));
}

#define DISK_CNT_ADDR 0x475
void disk_init(void) {
  put_str("disk_init start\n");
  uint8_t disk_cnt = *((uint8_t*)(DISK_CNT_ADDR));
  ASSERT((disk_cnt > 0) && (disk_cnt <= 4));
  ide_channel_init(disk_cnt);
  put_str("disk_init done\n");
}

