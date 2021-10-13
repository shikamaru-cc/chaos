#include "fs.h"
#include "debug.h"
#include "disk.h"
#include "kernel/bitmap.h"
#include "partition_manager.h"
#include "kernel/list.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "stdnull.h"
#include "stdbool.h"
#include "global.h"
#include "thread.h"
#include "memory.h"
#include "inode.h"

bool fs_load(struct partition_manager* fsm, struct partition* part);
void fs_make(struct partition_manager* fsm, struct partition* part);

// Public methods

void fs_init(void);

// --------------------------- Implementation ------------------------------- //

bool fs_load(struct partition_manager* fsm, struct partition* part) {
  ASSERT(part != NULL)
  fsm->part = part;

  struct super_block* sblock;
  sblock = (struct super_block*)sys_malloc(sizeof(struct super_block));

  // read super block
  disk_read(part->hd, sblock, part->lba_start + 1, 1);

  if (sblock->magic != SUPER_BLOCK_MAGIC) {
    sys_free(sblock);
    return false;
  }

  fsm->sblock = sblock;

  // Load inode bitmap
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;
  inode_btmp_ptr->btmp_bytes_len = sblock->inode_btmp_secs * 512;
  inode_btmp_ptr->bits = (uint8_t*)sys_malloc(inode_btmp_ptr->btmp_bytes_len);
  disk_read(part->hd, inode_btmp_ptr->bits, sblock->inode_btmp_lba, sblock->inode_btmp_secs);

  // Load block bitmap
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;
  block_btmp_ptr->btmp_bytes_len = sblock->block_btmp_secs * 512;
  block_btmp_ptr->bits = (uint8_t*)sys_malloc(block_btmp_ptr->btmp_bytes_len);
  disk_read(part->hd, block_btmp_ptr->bits, sblock->block_btmp_lba, sblock->block_btmp_secs);

  return true;
}

// FIXME: check whether malloc in fs_make use kmalloc
void fs_make(struct partition_manager* fsm, struct partition* part) {
  ASSERT(part != NULL)
  fsm->part = part;

  // Init super block
  struct super_block* sblock;
  // FIXME: Currently we use fs_make before we get into user mode, it is better
  // to use a new malloc call for kernel mode
  sblock = (struct super_block*)sys_malloc(sizeof(struct super_block));

  sblock->magic = SUPER_BLOCK_MAGIC;
  sblock->sec_cnt = part->sec_cnt;
  sblock->inode_cnt = FS_INODE_CNT;
  sblock->part_lba_start = part->lba_start;

  sblock->block_btmp_lba = part->lba_start + 1 + BLOCK_SECS;
  sblock->block_btmp_secs = \
    DIV_ROUND_UP(part->sec_cnt / BLOCK_SECS, BLOCK_BITS) * BLOCK_SECS;

  sblock->inode_btmp_lba = sblock->block_btmp_lba + sblock->block_btmp_secs;
  sblock->inode_btmp_secs = \
    DIV_ROUND_UP(FS_INODE_CNT, BLOCK_BITS) * BLOCK_SECS;

  sblock->inode_table_lba = sblock->inode_btmp_lba + sblock->inode_btmp_secs;
  sblock->inode_table_secs = \
    DIV_ROUND_UP(FS_INODE_CNT, FS_INODE_TABLES_BLOCK_CNT) * BLOCK_SECS;

  sblock->data_lba = sblock->inode_table_lba + sblock->inode_table_secs;
  sblock->root_inode_no = 0;
  // TODO: init dir_entry_size

  // flush super block
  disk_write(part->hd, sblock, part->lba_start + 1, 1);

  fsm->sblock = sblock;

  // Init inode bitmap
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;
  inode_btmp_ptr->btmp_bytes_len = sblock->inode_btmp_secs * 512;
  inode_btmp_ptr->bits = (uint8_t*)sys_malloc(inode_btmp_ptr->btmp_bytes_len);
  bitmap_init(inode_btmp_ptr);

  // set root inode no
  bitmap_set(inode_btmp_ptr, sblock->root_inode_no);

  // flush all inode bitmap
  disk_write(part->hd, inode_btmp_ptr->bits, sblock->inode_btmp_lba, sblock->inode_btmp_secs);

  // Init block bitmap
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;
  block_btmp_ptr->btmp_bytes_len = sblock->block_btmp_secs * 512;
  block_btmp_ptr->bits = (uint8_t*)sys_malloc(block_btmp_ptr->btmp_bytes_len);
  bitmap_init(block_btmp_ptr);

  // Set used block
  int32_t i, used_block;
  used_block = sblock->data_lba - sblock->part_lba_start;
  for (i = 0; i < used_block; i++) {
    bitmap_set(block_btmp_ptr, i);
  }

  // Our block bitmap size may be larger than actual block count,
  // so we need to set the tail non-exist block index to be used.
  int32_t block_btmp_size = block_btmp_ptr->btmp_bytes_len * 8;
  for (i = sblock->sec_cnt; i < block_btmp_size; i++) {
    bitmap_set(block_btmp_ptr, i);
  }

  // flush all block bitmap
  disk_write(part->hd, block_btmp_ptr->bits, sblock->block_btmp_lba, sblock->block_btmp_secs);

  // init root dir
  struct inode_elem root_inode_elem;
  root_inode_elem.partmgr = fsm;
  struct inode* root_inode = &root_inode_elem.inode;
  root_inode->no = sblock->root_inode_no;
  root_inode->size = 0;

  for (i = 0; i < FS_INODE_NUM_BLOCKS; i++) {
    root_inode->blocks[i] = 0;
  }

  inode_sync(&root_inode_elem);

  return;
}
void fs_init(void) {
  printf("fs_init start\n");
  ASSERT(!list_empty(&disk_partitions));
  struct list_elem* part_tag = list_top(&disk_partitions);
  struct partition* part;
  part = elem2entry(struct partition, tag, part_tag);

  // Init opened inode list
  list_init(&inode_list);

  if (!fs_load(&cur_partition, part)) {
    printf("  make default file system\n");
    fs_make(&cur_partition, part);
  }

  dir_open_root(&cur_partition);

  // FIXME: Test only
  struct dir_entry de;

  strcpy(de.filename, "chloe");
  de.f_type = TYPE_NORMAL;
  de.inode_no = get_free_inode_no(&cur_partition);
  dir_create_entry(&dir_root, &de);

  strcpy(de.filename, "shikamaru");
  de.f_type = TYPE_NORMAL;
  de.inode_no = get_free_inode_no(&cur_partition);
  dir_create_entry(&dir_root, &de);

  struct dir_entry de2;
  dir_search(&dir_root, "shikamaru", &de2);
  printf("filename: %s, type: %d, inode_no: %d", de2.filename, de2.f_type, de2.inode_no);
  
  // FIXME: end test code

  printf("  mount %s as default file system\n", cur_partition.part->name);
  printf("fs_init done\n");
}
