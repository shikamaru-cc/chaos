#include "fs.h"
#include "debug.h"
#include "disk.h"
#include "kernel/bitmap.h"
#include "stdio.h"
#include "string.h"
#include "stdnull.h"
#include "stdbool.h"
#include "global.h"
#include "thread.h"
#include "memory.h"
#include <stdint.h>

// -------------------------- Struct fs_manager ----------------------------- //

// Private methods

bool fs_load(struct fs_manager* fsm, struct partition* part);

void fs_make(struct fs_manager* fsm, struct partition* part);

void fs_inode_sync(struct fs_manager* fsm, struct inode_elem* inode_elem);

struct inode_elem* fs_inode_open(struct fs_manager* fsm, uint32_t inode_no);

void fs_inode_close(struct inode_elem* inode_elem);

int fs_alloc_inode_no(struct fs_manager* fsm);

void fs_free_inode_no(struct fs_manager* fsm, int inode_no);

void fs_sync_inode_no(struct fs_manager* fsm, int inode_no);

int fs_alloc_block_no(struct fs_manager* fsm);

void fs_free_block_no(struct fs_manager* fsm, int block_no);

void fs_sync_block_no(struct fs_manager* fsm, int block_no);

// Public methods

void fs_init(void);

// --------------------------- Implementation ------------------------------- //

bool fs_load(struct fs_manager* fsm, struct partition* part) {
  ASSERT(part != NULL)
  fsm->part = part;

  struct super_block* sblock;
  sblock = (struct super_block*)sys_malloc(sizeof(struct super_block));

  // read super block
  disk_read(part->hd, sblock, part->lba_start + 1, 1);

  if (sblock->magic != FS_SUPER_BLOCK_MAGIC) {
    sys_free(sblock);
    return false;
  }

  fsm->sblock = sblock;

  // Init opened inode list
  list_init(&fsm->inode_list);

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

void fs_make(struct fs_manager* fsm, struct partition* part) {
  ASSERT(part != NULL)
  fsm->part = part;

  // Init super block
  struct super_block* sblock;
  // FIXME: Currently we use fs_make before we get into user mode, it is better
  // to use a new malloc call for kernel mode
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

  fsm->sblock = sblock;

  // Init opened inode list
  list_init(&fsm->inode_list);

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
  int i, used_block;
  used_block = sblock->data_lba - sblock->part_lba_start;
  for (i = 0; i < used_block; i++) {
    bitmap_set(block_btmp_ptr, i);
  }

  // Our block bitmap size may be larger than actual block count,
  // so we need to set the tail non-exist block index to be used.
  uint32_t block_btmp_size = block_btmp_ptr->btmp_bytes_len * 8;
  for (i = sblock->sec_cnt; i < block_btmp_size; i++) {
    bitmap_set(block_btmp_ptr, i);
  }

  // flush all block bitmap
  disk_write(part->hd, block_btmp_ptr->bits, sblock->block_btmp_lba, sblock->block_btmp_secs);

  return;
}

void fs_inode_sync(struct fs_manager* fsm, struct inode_elem* inode_elem) {
  struct inode* inode = &inode_elem->inode;

  // locale the block where the inode lives
  uint32_t block_no = inode->no / FS_INODE_TABLES_BLOCK_CNT;
  uint32_t block_off = inode->no % FS_INODE_TABLES_BLOCK_CNT;
  uint32_t lba = fsm->sblock->inode_table_lba + block_no * FS_BLOCK_SECS;
  ASSERT(lba < fsm->sblock->inode_table_lba + fsm->sblock->inode_btmp_secs);

  // load block
  // FIXME: this malloc may allocate memory in user space
  struct inode* inode_table = (struct inode*)sys_malloc(FS_BLOCK_SIZE);
  disk_read(fsm->part->hd, inode_table, lba, FS_BLOCK_SECS);

  // copy new inode
  struct inode* inode_in_disk = &inode_table[block_off];
  memcpy(inode_in_disk, inode, sizeof(struct inode));

  // flush new inode
  disk_write(fsm->part->hd, inode_table, lba, FS_BLOCK_SECS);
  sys_free(inode_table);
}

struct inode_elem* fs_inode_open(struct fs_manager* fsm, uint32_t inode_no) {
  struct list_elem* elem = fsm->inode_list.head.next;
  struct inode_elem* inode_elem;

  while(elem != &fsm->inode_list.tail) {
    inode_elem = elem2entry(struct inode_elem, inode_tag, elem);
    if (inode_elem->inode.no == inode_no) {
      inode_elem->ref++;
      return inode_elem;
    }
    elem = elem->next;
  }

  // NOTE: Change current page dir to malloc memory in kernel space
  struct task_struct* cur = running_thread();
  uint32_t* cur_pgdir = cur->pgdir;
  cur->pgdir = NULL;
  inode_elem = (struct inode_elem*)sys_malloc(sizeof(struct inode_elem));
  cur->pgdir = cur_pgdir;

  // locale the block where the inode lives
  uint32_t block_no = inode_no / FS_INODE_TABLES_BLOCK_CNT;
  uint32_t block_off = inode_no % FS_INODE_TABLES_BLOCK_CNT;
  uint32_t lba = fsm->sblock->inode_table_lba + block_no * FS_BLOCK_SECS;
  ASSERT(lba < fsm->sblock->inode_table_lba + fsm->sblock->inode_btmp_secs);

  // load block
  // FIXME: this malloc may allocate memory in user space
  struct inode* inode_table = (struct inode*)sys_malloc(FS_BLOCK_SIZE);
  disk_read(fsm->part->hd, inode_table, lba, FS_BLOCK_SECS);

  // copy to memory
  struct inode* inode = &inode_elem->inode;
  struct inode* inode_in_disk = &inode_table[block_off];
  memcpy(inode, inode_in_disk, sizeof(struct inode));

  list_push(&fsm->inode_list, &inode_elem->inode_tag);

  sys_free(inode_table);
  return inode_elem;
}

void fs_inode_close(struct inode_elem* inode_elem) {
  inode_elem->ref--;
  if (inode_elem->ref == 0) {
    list_remove(&inode_elem->inode_tag);
    // NOTE: Change current page dir to free memory in kernel space
    struct task_struct* cur = running_thread();
    uint32_t* cur_pgdir = cur->pgdir;
    cur->pgdir = NULL;
    sys_free(inode_elem);
    cur->pgdir = cur_pgdir;
  }
}

// NOTE: fs_alloc_inode_no and fs_free_inode_no only modify inode bitmap in
// memory with no operation with disk.
int fs_alloc_inode_no(struct fs_manager* fsm) {
  int free_inode_no;
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;

  free_inode_no = bitmap_scan(inode_btmp_ptr, 1);
  if (free_inode_no < 0) {
    return -1;
  }

  bitmap_set(inode_btmp_ptr, free_inode_no);
  return free_inode_no;
}

void fs_free_inode_no(struct fs_manager* fsm, int inode_no) {
  // Not allow inode_no == 0, which is the root inode no
  ASSERT(inode_no > 0);
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;
  bitmap_unset(inode_btmp_ptr, inode_no);
}

void fs_sync_inode_no(struct fs_manager* fsm, int inode_no) {
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;
  // which block contains this inode no
  int block_off = inode_no / FS_BLOCK_BITS;
  uint32_t btmp_lba = fsm->sblock->inode_btmp_lba + block_off;
  // find corresponding bits block in memory
  uint32_t bytes_off = block_off * FS_BLOCK_SIZE;
  ASSERT(bytes_off < inode_btmp_ptr->btmp_bytes_len);
  void* bits_start = inode_btmp_ptr->bits + bytes_off;
  // sync
  disk_write(fsm->part->hd, bits_start, btmp_lba, 1);
}

int fs_alloc_block_no(struct fs_manager* fsm) {
  int free_block_no;
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;

  free_block_no = bitmap_scan(block_btmp_ptr, 1);
  if (free_block_no < 0) {
    return -1;
  }

  bitmap_set(block_btmp_ptr, free_block_no);
  return free_block_no;
}

void fs_free_block_no(struct fs_manager* fsm, int block_no) {
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;
  bitmap_unset(block_btmp_ptr, block_no);
}

void fs_sync_block_no(struct fs_manager* fsm, int block_no) {
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;
  // which block contains this block no
  int block_off = block_no / FS_BLOCK_BITS;
  uint32_t btmp_lba = fsm->sblock->block_btmp_lba + block_off;
  // find corresponding bits block in memory
  uint32_t bytes_off = block_off * FS_BLOCK_SIZE;
  ASSERT(bytes_off < block_btmp_ptr->btmp_bytes_len);
  void* bits_start = block_btmp_ptr->bits + bytes_off;
  // sync
  disk_write(fsm->part->hd, bits_start, btmp_lba, 1);
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

  // FIXME: Test only
  struct fs_manager* fsm = &fsm_default;
  int i;
  int block_no;
  for (i = 1; i < 10; i++) {
    block_no = fs_alloc_block_no(fsm);
    if (block_no < 0) {
      PANIC("no free inode no");
    }
    fs_sync_block_no(fsm, block_no);
  }

  printf("  mount %s as default file system\n", fsm_default.part->name);
  printf("fs_init done\n");
}
