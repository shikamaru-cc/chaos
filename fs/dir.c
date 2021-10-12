#include "dir.h"
#include "inode.h"
#include "string.h"
#include "stdnull.h"
#include "memory.h"
#include "disk.h"

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
  for (i = 0; i < FS_INODE_TOTAL_BLOCKS; i++) {
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
