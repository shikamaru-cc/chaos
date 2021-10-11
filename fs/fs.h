#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"
#include "stdbool.h"
#include "disk.h"
#include "kernel/list.h"
#include "kernel/bitmap.h"
#include "super_block.h"
#include "partition_manager.h"
#include "inode.h"

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

struct file {
  int32_t fd_flag;
  uint32_t fd_pos;
  struct inode_elem* inode_elem;
};

void fs_init(void);

#endif
