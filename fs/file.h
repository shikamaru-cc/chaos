#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "dir.h"
#include "stdint.h"

struct file {
  int32_t fd_flag;
  uint32_t fd_pos;
  struct inode_elem* inode_elem;
};

#define MAX_FILE 1024
struct file file_table[MAX_FILE];

extern void file_table_init(void);

extern int32_t file_create(struct dir* parent, char* filename);
extern int32_t file_open(uint32_t inode_no, int32_t flags);

#endif
