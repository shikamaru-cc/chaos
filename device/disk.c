#include "disk.h"
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

// Private methods

void ide_channel_rw(struct ide_channel* ide, uint8_t cmd, uint32_t lba, \
  uint32_t sec_cnt, enum disk_type dt, void* data);

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
  device |= (lbd >> 24);
  if (dt == DISK_MASTER) {
    device |= IDE_DEV_MASTER;
  } else {
    device |= IDE_DEV_SLAVE;
  }
  outb(ide_channel_dev(ide), device);

  return;
}

void ide_channel_read(struct ide_channel* ide, uint32_t lba, uint32_t sec_cnt, \
  enum disk_type dt, void* data) {
  // Two disk share one channel, we need lock
  lock_acquire(&ide->lock);

  uint8_t nsec = 0;
  while (sec_cnt != 0) {
    // We can only write 255 sectors each time, since the sec_cnt reg is 8 bit
    if sec_cnt > 255 {
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

    // Update lba and data ptr for next loop
    lba += nsec;
    data += wordcount;
  }

  lock_release(&ide->lock);
  return;
}

void ide_channel_write(struct ide_channel* ide, uint32_t lba, uint32_t sec_cnt, \
  enum disk_type dt, void* data) {
  // Two disk share one channel, we need lock
  lock_acquire(&ide->lock);

  uint8_t nsec = 0;
  while (sec_cnt != 0) {
    // We can only write 255 sectors each time, since the sec_cnt reg is 8 bit
    if sec_cnt > 255 {
      nsec = 255;
      sec_cnt -= 255;
    } else {
      nsec = sec_cnt
      sec_cnt = 0;
    }

    ide_channel_setup(ide, lba, nsec, dt);

    // Write command write
    outb(ide_channel_cmd(ide), IDE_CMD_WRITE);

    // Wait disk interrupt to wake me up
    ide->user_thread = running_thread();
    thread_block(TASK_BLOCKED);

    // Update lba and data ptr for next loop
    lba += nsec;
    data += wordcount;
  }

  lock_release(&ide->lock);
  return;
}

static void intr_disk_handler(void) {



}









