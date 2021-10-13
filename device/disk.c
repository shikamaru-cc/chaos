#include "disk.h"

#include "debug.h"
#include "interrupt.h"
#include "kernel/io.h"
#include "kernel/print.h"
#include "stdio.h"
#include "stdnull.h"
#include "string.h"
#include "sync.h"

// --------------------------- Struct ide_channel --------------------------- //
// defined in disk.h

// Macros

#define ide_channel_data(ide) (ide->port_base + 0)
#define ide_channel_error(ide) (ide->port_base + 1)
#define ide_channel_feature(ide) (ide->port_base + 1)
#define ide_channel_sec(ide) (ide->port_base + 2)
#define ide_channel_lba1(ide) (ide->port_base + 3)
#define ide_channel_lba2(ide) (ide->port_base + 4)
#define ide_channel_lba3(ide) (ide->port_base + 5)
#define ide_channel_dev(ide) (ide->port_base + 6)
#define ide_channel_status(ide) (ide->port_base + 7)
#define ide_channel_cmd(ide) (ide->port_base + 7)

#define IDE_DEV_MBS1 (1 << 7)
#define IDE_DEV_MOD_LBA (1 << 6)
#define IDE_DEV_MBS2 (1 << 5)
#define IDE_DEV_MASTER (0 << 4)
#define IDE_DEV_SLAVE (1 << 4)

#define IDE_CMD_READ 0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_IDENTIFY 0xEC

#define IDE_STATUS_BSY (1 << 7)
#define IDE_STATUS_DRQ (1 << 3)

// Private variables

// FIXME: We only support 2 ide channels yet

#define IDE_MAX_IDE_CHANNLES 2

struct ide_channel ide_channels[IDE_MAX_IDE_CHANNLES];

// Private methods

static void ide_channel_setup(struct ide_channel* ide, uint32_t lba,
                              uint8_t sec_cnt, enum disk_type dt);

static void ide_channel_pio(struct ide_channel* ide);

static void ide_channel_identify(struct ide_channel* ide, enum disk_type dt,
                                 char buf[512]);

static void ide_channel_read(struct ide_channel* ide, uint32_t lba,
                             uint32_t sec_cnt, enum disk_type dt, void* buf);

static void ide_channel_write(struct ide_channel* ide, uint32_t lba,
                              uint32_t sec_cnt, enum disk_type dt, void* buf);

static void ide_channel_init(uint32_t ide_channel_cnt);

static void intr_disk_handler(uint8_t irq_no);

// -------------------------------- Struct disk ----------------------------- //

// Private variables

uint8_t disk_cnt;

// Private methods

static void disk_swap_pairbytes(char* buf, uint32_t buflen);

static void disk_identify(struct disk* hd);

static void disk_init_per_disk(uint8_t disk_no, struct ide_channel* ide,
                               enum disk_type dt);

// Public methods

void disk_read(struct disk* hd, void* buf, uint32_t lba, uint32_t sec_cnt);

void disk_write(struct disk* hd, void* buf, uint32_t lba, uint32_t sec_cnt);

void disk_init(void);

// --------------------------- Struct partition ----------------------------- //

// Private struct for partition entry

#define FS_TYPE_NONE 0x0
#define FS_TYPE_EXTEND 0x5
#define FS_TYPE_LINUX 0x83

struct partition_table_entry {
  uint8_t bootable;
  uint8_t start_head;
  uint8_t start_sec;
  uint8_t start_chs;
  uint8_t fs_type;
  uint8_t end_head;
  uint8_t end_sec;
  uint8_t end_chs;
  uint32_t lba_start;
  uint32_t sec_cnt;
} __attribute__((packed));

struct boot_sector {
  uint8_t other[446];
  struct partition_table_entry partition_table[4];
  uint16_t signature;  // 0x55, 0xaa
} __attribute__((packed));

// Private methods

static struct partition* new_partition(struct disk* hd, uint32_t lba_start,
                                       uint32_t sec_cnt, uint8_t fs_type,
                                       uint32_t p_no);

static void partition_scan(struct disk* hd, uint32_t lba);

static void partition_string(struct list_elem* elem);

static void partition_printall(void);

static void partition_init(void);

// ------------------------------ Implementation ---------------------------- //

// ide_channel_setup
// Setup ide channel's sector count, lba and device register before sending
// command.
void ide_channel_setup(struct ide_channel* ide, uint32_t lba, uint8_t sec_cnt,
                       enum disk_type dt) {
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

// ide_channel_pio
// Polling ide channel status reg, at most wait for 30 seconds
void ide_channel_pio(struct ide_channel* ide) {
  static uint32_t max_wait_msecs = 30 * 1000;
  static uint32_t sleep_msecs = 10;
  uint32_t wait_msecs = 0;
  uint8_t status, err;

  while (wait_msecs < max_wait_msecs) {
    err = inb(ide_channel_error(ide));
    if (err != 0) {
      PANIC("ide channel error happen");
    }

    status = inb(ide_channel_status(ide));
    bool disk_busy = (status & IDE_STATUS_BSY) > 0;
    bool trans_ready = (status & IDE_STATUS_DRQ) > 0;

    if (!disk_busy && trans_ready) {
      return;
    }

    sys_milisleep(sleep_msecs);
    wait_msecs += sleep_msecs;
  }

  PANIC("ide_channel_pio bug: wait for disk ready over 30 seconds");
}

static void ide_channel_identify(struct ide_channel* ide, enum disk_type dt,
                                 char buf[512]) {
  // Write Device
  uint8_t device = IDE_DEV_MBS1 | IDE_DEV_MOD_LBA | IDE_DEV_MBS2;
  if (dt == DISK_MASTER) {
    device |= IDE_DEV_MASTER;
  } else {
    device |= IDE_DEV_SLAVE;
  }
  outb(ide_channel_dev(ide), device);
  outb(ide_channel_cmd(ide), IDE_CMD_IDENTIFY);
  sem_wait(&ide->sem);
  insw(ide_channel_data(ide), buf, 256 /* word count */);
}

void ide_channel_read(struct ide_channel* ide, uint32_t lba, uint32_t sec_cnt,
                      enum disk_type dt, void* buf) {
  // Two disk share one channel, we need lock
  lock_acquire(&ide->lock);

  uint8_t nsec = 0;
  while (sec_cnt != 0) {
    // We can only write 255 sectors each time, since the sec_cnt reg is 8 bit
    if (sec_cnt > 255) {
      nsec = 255;
      sec_cnt -= 255;
    } else {
      nsec = sec_cnt;
      sec_cnt = 0;
    }

    ide_channel_setup(ide, lba, nsec, dt);

    // Write command read
    outb(ide_channel_cmd(ide), IDE_CMD_READ);

    // Wait disk interrupt
    sem_wait(&ide->sem);

    uint32_t wordcount = nsec * 512 / 2;
    insw(ide_channel_data(ide), buf, wordcount);

    // Update lba and data ptr for next loop
    lba += nsec;
    buf += wordcount * 2;
  }

  lock_release(&ide->lock);
  return;
}

void ide_channel_write(struct ide_channel* ide, uint32_t lba, uint32_t sec_cnt,
                       enum disk_type dt, void* buf) {
  // Two disk share one channel, we need lock
  lock_acquire(&ide->lock);

  uint8_t nsec = 0;
  while (sec_cnt != 0) {
    // We can only write 255 sectors each time, since the sec_cnt reg is 8 bit
    if (sec_cnt > 255) {
      nsec = 255;
      sec_cnt -= 255;
    } else {
      nsec = sec_cnt;
      sec_cnt = 0;
    }

    ide_channel_setup(ide, lba, nsec, dt);

    // Write command write
    outb(ide_channel_cmd(ide), IDE_CMD_WRITE);

    ide_channel_pio(ide);

    uint32_t wordcount = nsec * 512 / 2;
    outsw(ide_channel_data(ide), buf, wordcount);

    // Wait for finishing writing
    sem_wait(&ide->sem);

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
    sem_init(&ide->sem, 0);
    register_handler(0x2e, intr_disk_handler);
  }

  if (ide_channel_cnt > 1) {
    ide = &ide_channels[1];
    ide->port_base = 0x170;
    ide->irq_no = 0x2f;
    lock_init(&ide->lock);
    sem_init(&ide->sem, 0);
    register_handler(0x2f, intr_disk_handler);
  }

  return;
}

static void intr_disk_handler(uint8_t irq_no) {
  ASSERT(irq_no == 0x2e || irq_no == 0x2f);
  uint8_t ch_no = irq_no - 0x2e;
  struct ide_channel* ide = &ide_channels[ch_no];
  ASSERT(ide->irq_no == irq_no);
  sem_post(&ide->sem);
  // inform disk interrupt has been handled;
  inb(ide_channel_status(ide));
}

// swap_pairbytes
// Helper function for disk_identify
static void disk_swap_pairbytes(char* buf, uint32_t buflen) {
  char tmp;
  uint32_t i;
  for (i = 0; i < buflen; i += 2) {
    tmp = buf[i];
    buf[i] = buf[i + 1];
    buf[i + 1] = tmp;
  }
}

// disk_identify
// Load disk id info into disk struct
static void disk_identify(struct disk* hd) {
  char buf[512];
  ide_channel_identify(hd->ide, hd->dt, buf);

  uint8_t seq_start = 10 * 2, seq_len = 20;
  memcpy(hd->seq, buf[seq_start], seq_len);
  disk_swap_pairbytes(hd->seq, seq_len);
  hd->seq[seq_len] = '\0';

  uint8_t mod_start = 27 * 2, mod_len = 40;
  memcpy(hd->module, buf[mod_start], mod_len);
  disk_swap_pairbytes(hd->module, mod_len);
  hd->module[mod_len] = '\0';

  uint8_t sec_start = 60 * 2;
  hd->sec_total = *((uint32_t*)&buf[sec_start]);
}

void disk_read(struct disk* hd, void* buf, uint32_t lba, uint32_t sec_cnt) {
  ide_channel_read(hd->ide, lba, sec_cnt, hd->dt, buf);
}

void disk_write(struct disk* hd, void* buf, uint32_t lba, uint32_t sec_cnt) {
  ide_channel_write(hd->ide, lba, sec_cnt, hd->dt, buf);
}

static void disk_init_per_disk(uint8_t disk_no, struct ide_channel* ide,
                               enum disk_type dt) {
  struct disk* hd = &disks[disk_no];
  hd->ide = ide;
  hd->dt = dt;
  sprintf(hd->name, "hd%c", disk_no + 'a');
  disk_identify(hd);
  hd->part_cnt = 0;
  printf("  disk%d:\n", disk_no);
  printf("    name: %s\n", hd->name);
  printf("    sequence: %s\n", hd->seq);
  printf("    module: %s\n", hd->module);
  printf("    total sectors: %d\n", hd->sec_total);
  printf("    total size: %dMB\n", hd->sec_total * 512 / 1024 / 1024);
}

#define DISK_CNT_ADDR 0x475
void disk_init(void) {
  put_str("disk_init start\n");
  disk_cnt = *((uint8_t*)(DISK_CNT_ADDR));
  ASSERT((disk_cnt > 0) && (disk_cnt <= 4));
  ide_channel_init(disk_cnt);

  disk_init_per_disk(0, &ide_channels[0], DISK_MASTER);
  if (disk_cnt == 1) {
    goto done;
  }
  disk_init_per_disk(1, &ide_channels[0], DISK_SLAVE);
  if (disk_cnt == 2) {
    goto done;
  }
  disk_init_per_disk(2, &ide_channels[1], DISK_MASTER);
  if (disk_cnt == 3) {
    goto done;
  }
  disk_init_per_disk(3, &ide_channels[1], DISK_SLAVE);

done:
  partition_init();
  put_str("disk_init done\n");
}

static struct partition* new_partition(struct disk* hd, uint32_t lba_start,
                                       uint32_t sec_cnt, uint8_t fs_type,
                                       uint32_t p_no) {
  struct partition* p;
  p = (struct partition*)sys_malloc(sizeof(struct partition));
  p->hd = hd;
  p->lba_start = lba_start;
  p->sec_cnt = sec_cnt;
  p->fs_type = fs_type;
  sprintf(p->name, "%sp%d", hd->name, p_no);
  return p;
}

static void partition_scan(struct disk* hd, uint32_t lba) {
  struct boot_sector* bs;
  bs = (struct boot_sector*)sys_malloc(sizeof(struct boot_sector));

  disk_read(hd, (void*)bs, lba, 1);

  // check signature
  if (bs->signature != 0xaa55) {
    goto ret;
  }

  struct partition_table_entry* pte;
  struct partition* p;
  int i;
  for (i = 0; i < 4; i++) {
    pte = &bs->partition_table[i];
    switch (pte->fs_type) {
      case FS_TYPE_NONE:
        break;
      case FS_TYPE_EXTEND:
        partition_scan(hd, lba + pte->lba_start);
        break;
      case FS_TYPE_LINUX:
        p = new_partition(hd, lba + pte->lba_start, pte->sec_cnt, FS_TYPE_LINUX,
                          hd->part_cnt++);
        list_append(&disk_partitions, &p->tag);
    }
  }

ret:
  sys_free(bs);
  return;
}

static void partition_string(struct list_elem* elem) {
  struct partition* p = elem2entry(struct partition, tag, elem);
  ASSERT((p != NULL) && (p->fs_type != FS_TYPE_NONE));
  char type_name[8];
  switch (p->fs_type) {
    case FS_TYPE_EXTEND:
      sprintf(type_name, "EXTEND");
      break;
    case FS_TYPE_LINUX:
      sprintf(type_name, "LINUX");
      break;
  }

  printf("    %s device:%s start:%d sectors:%d size:%dMB type:%s\n", p->name,
         p->hd->name, p->lba_start, p->sec_cnt, p->sec_cnt * 512 / 1024 / 1024,
         type_name);

  return;
}

static void partition_printall(void) {
  printf("  partitions:\n");
  list_iterate(&disk_partitions, partition_string);
}

static void partition_init(void) {
  ASSERT((disk_cnt > 0) && (disk_cnt <= 4));
  list_init(&disk_partitions);

  int i;
  for (i = 0; i < disk_cnt; i++) {
    partition_scan(&disks[i], 0);
  }

  partition_printall();
}
