#include "dir.h"

#include <stdint.h>

#include "disk.h"
#include "inode.h"
#include "memory.h"
#include "stdnull.h"
#include "string.h"

void dir_open_root(struct partition_manager* fsm) {
  struct inode_elem* root_inode_elem =
      inode_open(fsm, fsm->sblock->root_inode_no);
  dir_root.inode_elem = root_inode_elem;
}

int32_t dir_create_entry(struct dir* parent, struct dir_entry* ent) {
  struct inode* parent_inode = &parent->inode_elem->inode;
  if (parent_inode->size >= DIR_MAX_ENTRY) {
    return -1;
  }

  struct dir_entry* dents = (struct dir_entry*)sys_malloc(BLOCK_SIZE);
  if (dents == NULL) {
    return -1;
  }

  uint32_t block_used = inode_block_used(parent->inode_elem);
  uint32_t slot = block_used * DIR_ENTRY_PER_BLOCK;

  // has free slot
  if (parent_inode->size < slot) {
    uint32_t i;
    for (i = 0; i < block_used; i++) {
      inode_read(parent->inode_elem, i, (char*)dents);

      uint32_t j;
      for (j = 0; j < DIR_ENTRY_PER_BLOCK; j++) {
        // find free slot
        if (dents[j].inode_no == 0) {
          dents[j].inode_no = ent->inode_no;
          dents[j].f_type = ent->f_type;
          strcpy(dents[j].filename, ent->filename);

          parent_inode->size++;
          inode_sync(parent->inode_elem);

          inode_write(parent->inode_elem, i, (char*)dents);
          sys_free(dents);
          return 0;
        }
      }
    }
  }

  // no free slot
  inode_get_blocks(parent->inode_elem, 1);
  memset(dents, 0, BLOCK_SIZE);

  dents[0].inode_no = ent->inode_no;
  dents[0].f_type = ent->f_type;
  strcpy(dents[0].filename, ent->filename);
  inode_write(parent->inode_elem, block_used, (char*)dents);

  parent_inode->size++;
  inode_sync(parent->inode_elem);

  sys_free(dents);
  return 0;
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
