#include "fs.h"
#include "debug.h"
#include "stdio.h"
#include "stdnull.h"
#include "stdbool.h"
#include "global.h"
#include "memory.h"

// ---------------------------- Struct inode -------------------------------- //

// -------------------------- Struct fs_manager ----------------------------- //

// Private variable

struct fs_manager fsm_default;

// Private methods

bool fs_load(struct fs_manager* fsm, struct partition* part);

void fs_make(struct fs_manager* fsm, struct partition* part);

// Public methods

void fs_init(void);

// --------------------------- Implementation ------------------------------- //

bool fs_load(struct fs_manager* fsm, struct partition* part) {
  ASSERT(part != NULL)
  struct super_block* sblock;
  sblock = (struct super_block*)sys_malloc(sizeof(struct super_block));

  // read super block
  disk_read(part->hd, sblock, part->lba_start + 1, 1);

  if (sblock->magic != FS_SUPER_BLOCK_MAGIC) {
    sys_free(sblock);
    return false;
  }

  fsm->part = part;
  fsm->sblock = sblock;
  return true;
}

void fs_make(struct fs_manager* fsm, struct partition* part) {
  struct super_block* sblock;
  sblock = (struct super_block*)sys_malloc(sizeof(struct super_block));

  sblock->magic = FS_SUPER_BLOCK_MAGIC;
  sblock->sec_cnt = part->sec_cnt;
  sblock->inode_cnt = FS_INODE_CNT;
  sblock->part_lba_start = part->lba_start;

  sblock->block_btmp_lba = part->lba_start + 1 + FS_BLOCK_SECS;
  sblock->block_btmp_secs = \
    DIV_ROUND_UP(part->sec_cnt / FS_BLOCK_SECS, FS_BLOCK_BITS) * FS_BLOCK_SECS;

  sblock->inode_btmp_lba = sblock->block_btmp_lba + sblock->block_btmp_secs;
  sblock->inode_btmp_secs = \
    DIV_ROUND_UP(FS_INODE_CNT, FS_BLOCK_BITS) * FS_BLOCK_SECS;

  sblock->inode_table_lba = sblock->inode_btmp_lba + sblock->inode_btmp_secs;
  sblock->inode_table_secs = \
    DIV_ROUND_UP(FS_INODE_CNT, FS_INODE_TABLES_BLOCK_CNT) * FS_BLOCK_SECS;

  sblock->data_lba = sblock->inode_table_lba + sblock->inode_table_secs;
  sblock->root_inode_no = 0;
  // TODO: init dir_entry_size

  // flush super block
  disk_write(part->hd, sblock, part->lba_start + 1, 1);

  fsm->part = part;
  fsm->sblock = sblock;

  return;
}

void fs_init(void) {
  printf("fs_init start\n");
  ASSERT(!list_empty(&disk_partitions));
  struct list_elem* part_tag = list_top(&disk_partitions);
  struct partition* part;
  part = elem2entry(struct partition, tag, part_tag);
  if (!fs_load(&fsm_default, part)) {
    printf("  make default file system\n");
    fs_make(&fsm_default, part);
  }
  printf("  mount %s as default file system\n", fsm_default.part->name);
  printf("fs_init done\n");
}
