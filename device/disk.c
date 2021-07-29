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

// ide_channel_rw
// Read or write given ide_channel, depending on cmd
void ide_channel_rw(struct ide_channel* ide, uint8_t cmd, uint32_t lba, \
  uint32_t sec_cnt, enum disk_type dt, void* data) {
  // Two disk share one channel, we need lock
  lock_acquire(&ide->lock);

  while (sec_cnt != 0) {
    // We can only write 255 sectors each time, since the sec_cnt reg 
    // is 8 bit. nsec is the number of sectors to write in this loop.
    uint8_t nsec = 0;

    // Write sector count
    if sec_cnt > 255 {
      nsec = 255;
      sec_cnt -= 255;
    } else {
      nsec = sec_cnt
      sec_cnt = 0;
    }
    outb(ide_channel_sec(ide), nsec);

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

    // Update lba
    lba += nsec;

    // Write command
    outb(ide_channel_cmd(ide), cmd);

    // Wait disk interrupt to wake me up
    ide->user_thread = running_thread();
    thread_block(TASK_BLOCKED);

    // Read or write data
    uint32_t wordcount = nsec * 512 / 2;
    switch (cmd) {
    case IDE_CMD_READ:
      insw(ide_channel_data(ide), data, wordcount);
      break;
    case IDE_CMD_WRITE:
      outsw(ide_channel_data(ide), data, wordcount);
      break;
    case IDE_CMD_IDEN:
      // TODO
      break;
    default:
      PANIC("Unknown ide channel cmd");
    }

    data += wordcount;
  }

  lock_release(&ide->lock);
  return;
}
