#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "stdint.h"
#include "partition_manager.h"

enum file_type {
  TYPE_DIR,
  TYPE_NORMAL,
};

#define FS_MAX_FILENAME 50
struct dir_entry {
  char filename[FS_MAX_FILENAME];
  enum file_type f_type;
  uint32_t inode_no;
} __attribute__ ((packed));

#define DIR_ENTRY_PER_BLOCK (BLOCK_SIZE / (sizeof(struct dir_entry)))
#define DIR_MAX_ENTRY       (DIR_ENTRY_PER_BLOCK * FS_INODE_MAX_SECTORS)

struct dir {
  struct inode_elem* inode_elem;
};

struct dir dir_root;

void dir_open_root(struct partition_manager* fsm);
int32_t dir_append_entry(struct dir* parent, struct dir_entry* ent);
int32_t dir_search(struct dir* parent, char* filename, struct dir_entry* ent);

#endif
