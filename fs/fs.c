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

// ------------------------------ Struct dir -------------------------------- //

void dir_open_root(struct partition_manager* fsm);

int32_t dir_append_entry(struct dir* parent, struct dir_entry* ent);

int32_t dir_search(struct dir* parent, char* filename, struct dir_entry* ent);

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

  for (i = 0; i < FS_INODE_NUM_SECTORS; i++) {
    root_inode->sectors[i] = 0;
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
  de.inode_no = fs_alloc_inode_no(&cur_partition);
  dir_append_entry(&dir_root, &de);

  strcpy(de.filename, "shikamaru");
  de.f_type = TYPE_NORMAL;
  de.inode_no = fs_alloc_inode_no(&cur_partition);
  dir_append_entry(&dir_root, &de);

  struct dir_entry de2;
  dir_search(&dir_root, "shikamaru", &de2);
  printf("filename: %s, type: %d, inode_no: %d", de2.filename, de2.f_type, de2.inode_no);
  
  // FIXME: end test code

  printf("  mount %s as default file system\n", cur_partition.part->name);
  printf("fs_init done\n");
}


void dir_open_root(struct partition_manager* fsm) {
  struct inode_elem* root_inode_elem = inode_open(fsm, fsm->sblock->root_inode_no);
  dir_root.inode_elem = root_inode_elem;
}

int32_t dir_append_entry(struct dir* parent, struct dir_entry* ent) {
  struct inode* parent_inode = &parent->inode_elem->inode;
  if (parent_inode->size >= DIR_MAX_ENTRY) {
    return -1;
  }

  uint32_t pos = parent_inode->size;

  int32_t sec_idx = pos / DIR_ENTRY_PER_BLOCK;
  int32_t sec_off = pos % DIR_ENTRY_PER_BLOCK;

  uint32_t block_lba = inode_get_or_create_sec(parent->inode_elem, sec_idx);
  if (block_lba == 0) {
    return -1;
  }

  struct dir_entry* dents = (struct dir_entry*)sys_malloc(BLOCK_SIZE);

  struct partition_manager* fsm = parent->inode_elem->partmgr;
  uint32_t real_lba = block_lba + fsm->part->lba_start;
  disk_read(fsm->part->hd, dents, real_lba, BLOCK_SECS);

  memcpy(&dents[sec_off], ent, sizeof(struct dir_entry));

  // sync inode table and data
  parent_inode->size++;
  inode_sync(parent->inode_elem);
  disk_write(fsm->part->hd, dents, real_lba, BLOCK_SECS);

  sys_free(dents);
  return 1;
}

int32_t dir_search(struct dir* parent, char* filename, struct dir_entry* ent) {
  if (strlen(filename) == 0) {
    return -1;
  }

  struct dir_entry* dents = (struct dir_entry*)sys_malloc(BLOCK_SIZE);
  if (dents == NULL) {
    return -1;
  }

  struct inode_elem* p_inode = parent->inode_elem;
  uint32_t cnt = 1;
  uint32_t i;
  for (i = 0; i < FS_INODE_MAX_SECTORS; i++) {
    if (inode_read(p_inode, i, (char*)dents) < 0) {
      goto fail;
    }

    uint32_t j;
    for (j = 0; j < DIR_ENTRY_PER_BLOCK; j++) {
      if (cnt > p_inode->inode.size) {
	goto fail;
      }

      if (strcmp(dents[j].filename, filename) == 0) {
	strcpy(ent->filename, dents[j].filename);
	ent->f_type = dents[j].f_type;
	ent->inode_no = dents[j].inode_no;
	goto success;
      }

      cnt++;
    }
  }

 fail:
  sys_free(dents);
  return -1;

 success:
  sys_free(dents);
  return 0;
}
