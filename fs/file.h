#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "dir.h"
#include "stdint.h"

struct file {
  int32_t fd_flag;
  int32_t fd_pos;
  struct inode_elem* inode_elem;
};

#define MAX_FILE 1024
struct file file_table[MAX_FILE];

#define EOF (-1)

#define stdin 0
#define stdout 1
#define stderr 2

extern void file_table_init(void);

extern int32_t file_create(struct dir* parent, char* filename);
extern int32_t file_open(uint32_t inode_no, int32_t flags);
extern int32_t file_close(int32_t global_fd);
extern int32_t file_write(int32_t global_fd, const void* buf, int32_t size);
extern int32_t file_read(int32_t global_fd, void* buf, int32_t size);

#endif
