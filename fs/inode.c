#include "inode.h"

#include "debug.h"
#include "disk.h"
#include "memory.h"
#include "partition_manager.h"
#include "stdbool.h"
#include "stdint.h"
#include "stdio.h"
#include "stdnull.h"
#include "string.h"
#include "super_block.h"

// Public
void inode_sync(struct inode_elem* inode_elem);
struct inode_elem* inode_create(struct partition_manager* pmgr,
                                uint32_t inode_no);
struct inode_elem* inode_open(struct partition_manager* pmgr,
                              uint32_t inode_no);
void inode_close(struct inode_elem* inode_elem);
int32_t inode_get_blocks(struct inode_elem* inode_elem, uint32_t nr);
uint32_t inode_block_used(struct inode_elem* inode_elem);
uint32_t inode_get_or_create_sec(struct inode_elem* inode_elem,
                                 uint32_t sec_idx);
uint32_t inode_idx_to_lba(struct inode_elem* inode_elem, uint32_t sec_idx);
int32_t inode_read(struct inode_elem* inode_elem, uint32_t sec_idx, char* buf);
int32_t inode_write(struct inode_elem* inode_elem, uint32_t sec_idx, char* buf);

// Private
int32_t inode_read_ext_blocks(struct inode_elem* inode_elem, char* buf);
int32_t inode_write_ext_blocks(struct inode_elem* inode_elem, char* buf);

// Implementation
void inode_sync(struct inode_elem* inode_elem) {
  struct inode* inode = &inode_elem->inode;
  struct partition_manager* pmgr = inode_elem->partmgr;

  // locale the block where the inode lives
  uint32_t block_no = inode->no / FS_INODE_TABLES_BLOCK_CNT;
  uint32_t block_off = inode->no % FS_INODE_TABLES_BLOCK_CNT;
  uint32_t lba = pmgr->sblock->inode_table_lba + block_no * BLOCK_SECS;
  ASSERT(lba < pmgr->sblock->inode_table_lba + pmgr->sblock->inode_btmp_secs);

  // load block
  // FIXME: this malloc may allocate memory in user space
  struct inode* inode_table = (struct inode*)sys_malloc(BLOCK_SIZE);
  disk_read(pmgr->part->hd, inode_table, lba, BLOCK_SECS);

  // copy new inode
  struct inode* inode_in_disk = &inode_table[block_off];
  memcpy(inode_in_disk, inode, sizeof(struct inode));

  // flush new inode
  disk_write(pmgr->part->hd, inode_table, lba, BLOCK_SECS);
  sys_free(inode_table);
}

struct inode_elem* inode_create(struct partition_manager* pmgr,
                                uint32_t inode_no) {
  if (inode_no >= FS_INODE_CNT) {
    printf("create inode greater than max");
    return NULL;
  }

  // inode_elem should be shared in kernel space
  struct inode_elem* inode_elem;
  inode_elem = (struct inode_elem*)kmalloc(sizeof(struct inode_elem));
  if (inode_elem == NULL) {
    return NULL;
  }

  // init inode
  struct inode* inode = &inode_elem->inode;
  inode->no = inode_no;
  inode->size = 0;

  int32_t i;
  for (i = 0; i < FS_INODE_NUM_BLOCKS; i++) {
    inode->blocks[i] = 0;
  }

  // other fileds
  inode_elem->partmgr = pmgr;
  list_append(&inode_list, &inode_elem->inode_tag);
  inode_elem->ref = 1;

  return inode_elem;
}

// open inode in default file manager
struct inode_elem* inode_open(struct partition_manager* pmgr,
                              uint32_t inode_no) {
  if (!validate_inode_no(pmgr, inode_no)) {
    return NULL;
  }

  struct list_elem* elem = inode_list.head.next;
  struct inode_elem* inode_elem;

  // search cache
  while (elem != &inode_list.tail) {
    inode_elem = elem2entry(struct inode_elem, inode_tag, elem);
    if (inode_elem->inode.no == inode_no) {
      inode_elem->ref++;
      return inode_elem;
    }
    elem = elem->next;
  }

  // read from disk
  inode_elem = (struct inode_elem*)kmalloc(sizeof(struct inode_elem));

  // locale the block where the inode lives
  uint32_t block_no = inode_no / FS_INODE_TABLES_BLOCK_CNT;
  uint32_t block_off = inode_no % FS_INODE_TABLES_BLOCK_CNT;
  uint32_t lba = pmgr->sblock->inode_table_lba + block_no * BLOCK_SECS;
  ASSERT(lba < pmgr->sblock->inode_table_lba + pmgr->sblock->inode_btmp_secs);

  // load block
  struct inode* inode_table = (struct inode*)sys_malloc(BLOCK_SIZE);
  disk_read(pmgr->part->hd, inode_table, lba, BLOCK_SECS);

  // copy to memory
  struct inode* inode = &inode_elem->inode;
  struct inode* inode_in_disk = &inode_table[block_off];
  memcpy(inode, inode_in_disk, sizeof(struct inode));
  // init other fileds
  inode_elem->partmgr = pmgr;
  list_push(&inode_list, &inode_elem->inode_tag);
  inode_elem->ref = 1;

  sys_free(inode_table);
  return inode_elem;
}

void inode_close(struct inode_elem* inode_elem) {
  inode_elem->ref--;
  if (inode_elem->ref == 0) {
    list_remove(&inode_elem->inode_tag);
    kfree(inode_elem);
  }
}

int32_t inode_get_blocks(struct inode_elem* inode_elem, uint32_t cnt) {
  uint32_t used = inode_block_used(inode_elem);
  if (used + cnt > FS_INODE_TOTAL_BLOCKS) {
    printf("inode_get_blocks: exceed max blocks\n");
    return -1;
  }

  int32_t blocks[cnt];
  uint32_t i;
  for (i = 0; i < cnt; i++) {
    blocks[i] = get_free_block_no(inode_elem->partmgr);
    // Not enough blocks, rollback
    if (blocks[i] < 0) {
      uint32_t j;
      for (j = 0; j < i; j++) {
        release_block_no(inode_elem->partmgr, blocks[j]);
      }
      return -1;
    }
  }

  // not use all direct blocks
  if (used + cnt <= FS_INODE_DIRECT_BLOCKS) {
    for (i = 0; i < cnt; i++) {
      inode_elem->inode.blocks[i + used] = blocks[i];
    }

    inode_sync(inode_elem);
    sync_block_btmp(inode_elem->partmgr);

    return 0;
  }

  // now we need to use extend block

  // we have used extend block
  if (used > FS_INODE_DIRECT_BLOCKS) {
    uint32_t* ext_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE);
    if (ext_blocks == NULL) {
      // rollback if fail
      for (i = 0; i < cnt; i++) {
        release_block_no(inode_elem->partmgr, blocks[i]);
      }
      return -1;
    }

    uint32_t ext_block_no =
        inode_elem->inode.blocks[FS_INODE_EXTEND_BLOCK_INDEX];
    ASSERT(ext_block_no > 0);

    inode_read_ext_blocks(inode_elem, (char*)ext_blocks);

    uint32_t ext_used = used - FS_INODE_DIRECT_BLOCKS;
    for (i = 0; i < cnt; i++) {
      ext_blocks[i + ext_used] = blocks[i];
    }

    inode_write_ext_blocks(inode_elem, (char*)ext_blocks);
    sync_block_btmp(inode_elem->partmgr);

    return 0;
  }

  // now we should write both direct block and extend block

  // create extend block
  uint32_t* ext_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE);
  if (ext_blocks == NULL) {
    // rollback if fail
    for (i = 0; i < cnt; i++) {
      release_block_no(inode_elem->partmgr, blocks[i]);
    }
    return -1;
  }
  memset(ext_blocks, 0, BLOCK_SIZE);

  int32_t ext_block_no = get_free_block_no(inode_elem->partmgr);
  if (ext_block_no < 0) {
    // rollback if fail
    sys_free(ext_blocks);
    for (i = 0; i < cnt; i++) {
      release_block_no(inode_elem->partmgr, blocks[i]);
    }
    return -1;
  }
  inode_elem->inode.blocks[FS_INODE_EXTEND_BLOCK_INDEX] = ext_block_no;

  uint32_t p = 0;
  for (i = used; i < FS_INODE_DIRECT_BLOCKS; i++) {
    inode_elem->inode.blocks[i] = blocks[p];
    p++;
  }

  for (i = 0; i < used + cnt - FS_INODE_DIRECT_BLOCKS; i++) {
    ext_blocks[i] = blocks[i + p];
  }

  inode_sync(inode_elem);
  inode_write_ext_blocks(inode_elem, (char*)ext_blocks);
  sync_block_btmp(inode_elem->partmgr);

  return 0;
}

uint32_t inode_block_used(struct inode_elem* inode_elem) {
  uint32_t cnt = 0;

  // count direct
  uint32_t i;
  for (i = 0; i < FS_INODE_NUM_BLOCKS - 1; i++) {
    if (inode_elem->inode.blocks[i] > 0) {
      cnt++;
    } else {
      return cnt;
    }
  }

  bool has_extend = inode_elem->inode.blocks[FS_INODE_EXTEND_BLOCK_INDEX] > 0;
  if (!has_extend) {
    return cnt;
  }

  // has indirect block
  uint32_t* ext_blocks;
  ext_blocks = (uint32_t*)sys_malloc(BLOCK_SIZE);
  inode_read_ext_blocks(inode_elem, (char*)ext_blocks);

  for (i = 0; i < FS_INODE_EXTEND_BLOCK_CNT; i++) {
    if (ext_blocks[i] == 0) {
      break;
    }
    cnt++;
  }

  sys_free(ext_blocks);
  return cnt;
}

uint32_t inode_get_or_create_sec(struct inode_elem* inode_elem,
                                 uint32_t sec_idx) {
  struct inode* inode = &inode_elem->inode;

  if (sec_idx >= FS_INODE_TOTAL_BLOCKS) {
    PANIC("sec_idx > max");
  }

  if (sec_idx < FS_INODE_EXTEND_BLOCK_INDEX) {
    if (inode->blocks[sec_idx] != 0) {
      return inode->blocks[sec_idx];
    }

    // map new block
    int32_t free_block_lba = get_free_block_no(inode_elem->partmgr);
    if (free_block_lba < 0) {
      return 0;
    }

    inode->blocks[sec_idx] = free_block_lba;
    inode_sync(inode_elem);
    return inode->blocks[sec_idx];
  }

  // locate at extend sectors

  bool exist = true;
  // alloc new block if extend block not exist
  if (inode->blocks[FS_INODE_EXTEND_BLOCK_INDEX] == 0) {
    exist = false;
    int32_t free_block_lba = get_free_block_no(inode_elem->partmgr);
    if (free_block_lba < 0) {
      return 0;
    }

    inode->blocks[FS_INODE_EXTEND_BLOCK_INDEX] = free_block_lba;
    inode_sync(inode_elem);
  }

  // load extend sector
  uint32_t ext_blk_lba = inode->blocks[FS_INODE_EXTEND_BLOCK_INDEX];
  uint32_t* ext_sec = (uint32_t*)sys_malloc(BLOCK_SIZE);
  struct partition_manager* pmgr = inode_elem->partmgr;
  uint32_t real_lba = pmgr->part->lba_start + ext_blk_lba;

  // init buf if extend block is not exist before
  if (!exist) {
    memset(ext_sec, 0, BLOCK_SIZE);
  } else {
    disk_read(pmgr->part->hd, ext_sec, real_lba, BLOCK_SECS);
  }

  uint32_t ext_sec_idx = sec_idx - FS_INODE_EXTEND_BLOCK_INDEX;
  if (ext_sec[ext_sec_idx] == 0) {
    int32_t free_block_lba = get_free_block_no(inode_elem->partmgr);

    if (free_block_lba < 0) {
      return 0;
    }

    // sync extend sector
    ext_sec[ext_sec_idx] = free_block_lba;
    disk_write(pmgr->part->hd, ext_sec, real_lba, BLOCK_SECS);
  }

  uint32_t ret = ext_sec[ext_sec_idx];
  sys_free(ext_sec);
  return ret;
}

uint32_t inode_idx_to_lba(struct inode_elem* inode_elem, uint32_t sec_idx) {
  if (sec_idx < FS_INODE_EXTEND_BLOCK_INDEX) {
    return inode_elem->inode.blocks[sec_idx];
  }

  // load from extend block
  uint32_t ext_blk_lba = inode_elem->inode.blocks[FS_INODE_EXTEND_BLOCK_INDEX];
  if (ext_blk_lba == 0) {
    return 0;
  }

  uint32_t* ext_sec = sys_malloc(BLOCK_SIZE);
  if (ext_sec == NULL) {
    return 0;
  }

  struct partition_manager* pmgr = inode_elem->partmgr;
  uint32_t real_lba = pmgr->part->lba_start + ext_blk_lba;

  disk_read(pmgr->part->hd, ext_sec, real_lba, BLOCK_SECS);

  uint32_t ret = ext_sec[sec_idx - FS_INODE_EXTEND_BLOCK_INDEX];
  sys_free(ext_sec);
  return ret;
}

int32_t inode_read(struct inode_elem* inode_elem, uint32_t sec_idx, char* buf) {
  uint32_t lba = inode_idx_to_lba(inode_elem, sec_idx);
  if (lba == 0) {
    return -1;
  }
  struct partition_manager* pmgr = inode_elem->partmgr;
  uint32_t real_lba = pmgr->part->lba_start + lba;
  disk_read(pmgr->part->hd, buf, real_lba, BLOCK_SECS);
  return 0;
}

int32_t inode_write(struct inode_elem* inode_elem, uint32_t sec_idx,
                    char* buf) {
  uint32_t lba = inode_idx_to_lba(inode_elem, sec_idx);
  if (lba == 0) {
    return -1;
  }
  struct partition_manager* pmgr = inode_elem->partmgr;
  uint32_t real_lba = pmgr->part->lba_start + lba;
  disk_write(pmgr->part->hd, buf, real_lba, BLOCK_SECS);
  return 0;
}

int32_t inode_read_ext_blocks(struct inode_elem* inode_elem, char* buf) {
  int32_t idx = inode_elem->inode.blocks[FS_INODE_EXTEND_BLOCK_INDEX];
  if (idx == 0) {
    return -1;
  }
  uint32_t real_lba = inode_elem->partmgr->part->lba_start + idx;
  disk_read(inode_elem->partmgr->part->hd, buf, real_lba, BLOCK_SECS);
  return 1;
}

int32_t inode_write_ext_blocks(struct inode_elem* inode_elem, char* buf) {
  int32_t idx = inode_elem->inode.blocks[FS_INODE_EXTEND_BLOCK_INDEX];
  if (idx == 0) {
    return -1;
  }
  uint32_t real_lba = inode_elem->partmgr->part->lba_start + idx;
  disk_write(inode_elem->partmgr->part->hd, buf, real_lba, BLOCK_SECS);
  return 1;
}
