#include "debug.h"
#include "stdnull.h"
#include "disk.h"
#include "inode.h"
#include "string.h"
#include "memory.h"

void inode_sync(struct inode_elem* inode_elem) {
  struct inode* inode = &inode_elem->inode;
  struct partition_manager* fsm = inode_elem->partmgr;

  // locale the block where the inode lives
  uint32_t block_no = inode->no / FS_INODE_TABLES_BLOCK_CNT;
  uint32_t block_off = inode->no % FS_INODE_TABLES_BLOCK_CNT;
  uint32_t lba = fsm->sblock->inode_table_lba + block_no * BLOCK_SECS;
  ASSERT(lba < fsm->sblock->inode_table_lba + fsm->sblock->inode_btmp_secs);

  // load block
  // FIXME: this malloc may allocate memory in user space
  struct inode* inode_table = (struct inode*)sys_malloc(BLOCK_SIZE);
  disk_read(fsm->part->hd, inode_table, lba, BLOCK_SECS);

  // copy new inode
  struct inode* inode_in_disk = &inode_table[block_off];
  memcpy(inode_in_disk, inode, sizeof(struct inode));

  // flush new inode
  disk_write(fsm->part->hd, inode_table, lba, BLOCK_SECS);
  sys_free(inode_table);
}

struct inode_elem* inode_create(struct partition_manager *fsm) {
  int32_t inode_no = fs_alloc_inode_no(fsm);
  struct inode_elem* inode_elem;

  if (inode_no < 0) {
    return NULL;
  }

  inode_elem = (struct inode_elem*)kmalloc(sizeof(struct inode_elem));

  // init inode
  struct inode* inode = &inode_elem->inode;
  inode->no = inode_no;
  inode->size = 0;

  int32_t i;
  for (i = 0; i < FS_INODE_NUM_SECTORS; i++) {
    inode->sectors[i] = 0;
  }

  inode_sync(inode_elem);

  // other fileds
  inode_elem->partmgr = fsm;
  list_append(&inode_list, &inode_elem->inode_tag);
  inode_elem->ref = 1;

  return inode_elem;
}

// open inode in default file manager
// FIXME: return null if inode no not create
struct inode_elem* inode_open(struct partition_manager* fsm, uint32_t inode_no) {
  struct list_elem* elem = inode_list.head.next;
  struct inode_elem* inode_elem;

  while(elem != &inode_list.tail) {
    inode_elem = elem2entry(struct inode_elem, inode_tag, elem);
    if (inode_elem->inode.no == inode_no) {
      inode_elem->ref++;
      return inode_elem;
    }
    elem = elem->next;
  }

  inode_elem = (struct inode_elem*)kmalloc(sizeof(struct inode_elem));

  // locale the block where the inode lives
  uint32_t block_no = inode_no / FS_INODE_TABLES_BLOCK_CNT;
  uint32_t block_off = inode_no % FS_INODE_TABLES_BLOCK_CNT;
  uint32_t lba = fsm->sblock->inode_table_lba + block_no * BLOCK_SECS;
  ASSERT(lba < fsm->sblock->inode_table_lba + fsm->sblock->inode_btmp_secs);

  // load block
  // FIXME: this malloc may allocate memory in user space
  struct inode* inode_table = (struct inode*)sys_malloc(BLOCK_SIZE);
  disk_read(fsm->part->hd, inode_table, lba, BLOCK_SECS);

  // copy to memory
  struct inode* inode = &inode_elem->inode;
  struct inode* inode_in_disk = &inode_table[block_off];
  memcpy(inode, inode_in_disk, sizeof(struct inode));
  // init other fileds
  inode_elem->partmgr = fsm;
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

uint32_t inode_get_or_create_sec(struct inode_elem* inode_elem, uint32_t sec_idx) {
  struct inode* inode = &inode_elem->inode;

  if (sec_idx >= FS_INODE_MAX_SECTORS) {
    PANIC("sec_idx > max");
  }

  if (sec_idx < FS_INODE_EXTEND_BLOCK_INDEX) {
    if (inode->sectors[sec_idx] != 0) {
      return inode->sectors[sec_idx];
    }

    // map new block
    int32_t free_block_lba = fs_alloc_block_no(inode_elem->partmgr);
    if (free_block_lba < 0) {
      return 0;
    }

    inode->sectors[sec_idx] = free_block_lba;
    inode_sync(inode_elem);
    return inode->sectors[sec_idx];
  }

  // locate at extend sectors

  bool exist = true;
  // alloc new block if extend block not exist
  if (inode->sectors[FS_INODE_EXTEND_BLOCK_INDEX] == 0) {
    exist  = false;
    int32_t free_block_lba = fs_alloc_block_no(inode_elem->partmgr);
    if (free_block_lba < 0) {
      return 0;
    }

    inode->sectors[FS_INODE_EXTEND_BLOCK_INDEX] = free_block_lba;
    inode_sync(inode_elem);
  }

  // load extend sector
  uint32_t ext_blk_lba = inode->sectors[FS_INODE_EXTEND_BLOCK_INDEX];
  uint32_t* ext_sec = (uint32_t*)sys_malloc(BLOCK_SIZE);
  struct partition_manager* fsm = inode_elem->partmgr;
  uint32_t real_lba = fsm->part->lba_start + ext_blk_lba;

  // init buf if extend block is not exist before
  if (!exist) {
    memset(ext_sec, 0, BLOCK_SIZE);
  } else {
    disk_read(fsm->part->hd, ext_sec, real_lba, BLOCK_SECS);
  }

  uint32_t ext_sec_idx = sec_idx - FS_INODE_EXTEND_BLOCK_INDEX;
  if (ext_sec[ext_sec_idx] == 0) {
    int32_t free_block_lba = fs_alloc_block_no(inode_elem->partmgr);

    if (free_block_lba < 0) {
      return 0;
    }

    // sync extend sector
    ext_sec[ext_sec_idx] = free_block_lba;
    disk_write(fsm->part->hd, ext_sec, real_lba, BLOCK_SECS);
  }

  uint32_t ret = ext_sec[ext_sec_idx];
  sys_free(ext_sec);
  return ret;
}

uint32_t inode_idx_to_lba(struct inode_elem* inode_elem, uint32_t sec_idx) {
  if (sec_idx < FS_INODE_EXTEND_BLOCK_INDEX) {
    return inode_elem->inode.sectors[sec_idx];
  }

  // load from extend block
  uint32_t ext_blk_lba = inode_elem->inode.sectors[FS_INODE_EXTEND_BLOCK_INDEX];
  if (ext_blk_lba == 0) {
    return 0;
  }

  uint32_t* ext_sec = sys_malloc(BLOCK_SIZE);
  if (ext_sec == NULL) {
    return 0;
  }

  struct partition_manager* fsm = inode_elem->partmgr;
  uint32_t real_lba = fsm->part->lba_start + ext_blk_lba;

  disk_read(fsm->part->hd, ext_sec, real_lba, BLOCK_SECS);

  uint32_t ret = ext_sec[sec_idx - FS_INODE_EXTEND_BLOCK_INDEX];
  sys_free(ext_sec);
  return ret;
}

int32_t inode_read(struct inode_elem* inode_elem, uint32_t sec_idx, char* buf) {
  uint32_t lba = inode_idx_to_lba(inode_elem, sec_idx);
  if (lba == 0) {
    return -1;
  }
  struct partition_manager* fsm = inode_elem->partmgr;
  uint32_t real_lba = fsm->part->lba_start + lba;
  disk_read(fsm->part->hd, buf, real_lba, BLOCK_SECS);
  return 0;
}

int32_t inode_write(struct inode_elem *inode_elem, uint32_t sec_idx, char *buf) {
  uint32_t lba = inode_idx_to_lba(inode_elem, sec_idx);
  if (lba == 0) {
    return -1;
  }
  struct partition_manager* fsm = inode_elem->partmgr;
  uint32_t real_lba = fsm->part->lba_start + lba;
  disk_write(fsm->part->hd, buf, real_lba, BLOCK_SECS);
  return 0;
}
