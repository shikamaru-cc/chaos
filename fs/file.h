#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "stdint.h"

struct file {
  int32_t fd_flag;
  uint32_t fd_pos;
  struct inode_elem* inode_elem;
};

#endif
