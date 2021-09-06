#include "fs.h"
#include "debug.h"
#include "stdio.h"
#include "string.h"
#include "stdnull.h"
#include "stdbool.h"
#include "global.h"
#include "thread.h"
#include "memory.h"

// -------------------------- Struct fs_manager ----------------------------- //

// Private variable

struct fs_manager fsm_default;

// Private methods

bool fs_load(struct fs_manager* fsm, struct partition* part);

void fs_make(struct fs_manager* fsm, struct partition* part);

void fs_inode_sync(struct fs_manager* fsm, struct inode_elem* inode_elem);

struct inode_elem* fs_inode_open(struct fs_manager* fsm, uint32_t inode_no);

void fs_inode_close(struct inode_elem* inode_elem);

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
    list_remove(inode_elem);
    // NOTE: Change current page dir to free memory in kernel space
    struct task_struct* cur = running_thread();
    uint32_t* cur_pgdir = cur->pgdir;
    cur->pgdir = NULL;
    sys_free(inode_elem);
    cur->pgdir = cur_pgdir;
  }
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
  // FIXME: use another method to do load or make, then init inode list
  list_init(&fsm_default.inode_list);
  printf("  mount %s as default file system\n", fsm_default.part->name);
  printf("fs_init done\n");
}
