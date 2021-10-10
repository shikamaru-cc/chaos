#include "fs.h"
#include "debug.h"
#include "disk.h"
#include "kernel/bitmap.h"
#include "kernel/list.h"
#include "stdio.h"
#include "stdint.h"
#include "string.h"
#include "stdnull.h"
#include "stdbool.h"
#include "global.h"
#include "thread.h"
#include "memory.h"

// -------------------------- Struct partition_manager ----------------------------- //

// Private methods

bool fs_load(struct partition_manager* fsm, struct partition* part);

void fs_make(struct partition_manager* fsm, struct partition* part);

void fs_inode_close(struct inode_elem* inode_elem);

int32_t fs_alloc_inode_no(struct partition_manager* fsm);

void fs_free_inode_no(struct partition_manager* fsm, int32_t inode_no);

void fs_sync_inode_no(struct partition_manager* fsm, int32_t inode_no);

int32_t fs_alloc_block_no(struct partition_manager* fsm);

void fs_free_block_no(struct partition_manager* fsm, int32_t block_no);

void fs_sync_block_no(struct partition_manager* fsm, int32_t block_no);

// Public methods

void fs_init(void);

// ---------------------------- Struct inode -------------------------------- //

void inode_sync(struct inode_elem* inode_elem);

struct inode_elem* inode_create(struct partition_manager* fsm);

struct inode_elem* inode_open(struct partition_manager* fsm, uint32_t inode_no);

void inode_close(struct inode_elem* inode_elem);

uint32_t inode_get_or_create_sec(struct inode_elem* inode_elem, uint32_t sec_idx);

uint32_t inode_idx_to_lba(struct inode_elem* inode_elem, uint32_t sec_idx);

int32_t inode_read(struct inode_elem* inode_elem, uint32_t sec_idx, char* buf);

int32_t inode_write(struct inode_elem* inode_elem, uint32_t sec_idx, char* buf);

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

// NOTE: fs_alloc_inode_no and fs_free_inode_no only modify inode bitmap in
// memory with no operation with disk.
int32_t fs_alloc_inode_no(struct partition_manager* fsm) {
  int32_t free_inode_no;
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;

  free_inode_no = bitmap_scan(inode_btmp_ptr, 1);
  if (free_inode_no < 0) {
    return -1;
  }

  bitmap_set(inode_btmp_ptr, free_inode_no);
  return free_inode_no;
}

void fs_free_inode_no(struct partition_manager* fsm, int32_t inode_no) {
  // Not allow inode_no == 0, which is the root inode no
  ASSERT(inode_no > 0);
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;
  bitmap_unset(inode_btmp_ptr, inode_no);
}

void fs_sync_inode_no(struct partition_manager* fsm, int32_t inode_no) {
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;
  // which block contains this inode no
  int32_t block_off = inode_no / BLOCK_BITS;
  uint32_t btmp_lba = fsm->sblock->inode_btmp_lba + block_off;
  // find corresponding bits block in memory
  uint32_t bytes_off = block_off * BLOCK_SIZE;
  ASSERT(bytes_off < inode_btmp_ptr->btmp_bytes_len);
  void* bits_start = inode_btmp_ptr->bits + bytes_off;
  // sync
  disk_write(fsm->part->hd, bits_start, btmp_lba, 1);
}

int32_t fs_alloc_block_no(struct partition_manager* fsm) {
  int32_t free_block_no;
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;

  free_block_no = bitmap_scan(block_btmp_ptr, 1);
  if (free_block_no < 0) {
    return -1;
  }

  bitmap_set(block_btmp_ptr, free_block_no);
  fs_sync_inode_no(fsm, free_block_no);
  return free_block_no;
}

void fs_free_block_no(struct partition_manager* fsm, int32_t block_no) {
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;
  bitmap_unset(block_btmp_ptr, block_no);
  fs_sync_inode_no(fsm, block_no);
}

void fs_sync_block_no(struct partition_manager* fsm, int32_t block_no) {
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;
  // which block contains this block no
  int32_t block_off = block_no / BLOCK_BITS;
  uint32_t btmp_lba = fsm->sblock->block_btmp_lba + block_off;
  // find corresponding bits block in memory
  uint32_t bytes_off = block_off * BLOCK_SIZE;
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
  list_append(&fsm->inode_list, &inode_elem->inode_tag);
  inode_elem->ref = 1;

  return inode_elem;
}

// open inode in default file manager
// FIXME: return null if inode no not create
struct inode_elem* inode_open(struct partition_manager* fsm, uint32_t inode_no) {
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
  list_push(&fsm->inode_list, &inode_elem->inode_tag);
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
