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
#include "dir.h"

struct file {
  int32_t fd_flag;
  uint32_t fd_pos;
  struct inode_elem* inode_elem;
};

void fs_init(void);

#endif
