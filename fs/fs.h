#ifndef __FS_FS_H
#define __FS_FS_H

#include "dir.h"
#include "disk.h"
#include "inode.h"
#include "kernel/bitmap.h"
#include "kernel/list.h"
#include "partition_manager.h"
#include "stdbool.h"
#include "stdint.h"
#include "super_block.h"

void fs_init(void);

// flags
#define O_CREATE (1 << 0)

#define SEEK_SET 1
#define SEEK_CUR 2
#define SEEK_END 3

extern int32_t sys_open(const char* pathname, int32_t flags);
extern int32_t sys_close(int32_t fd);
extern int32_t sys_write(int32_t fd, const void* buf, int32_t size);
extern int32_t sys_read(int32_t fd, void* buf, int32_t size);
extern int32_t sys_lseek(int32_t fd, int32_t offset, int32_t whence);
extern int32_t sys_unlink(const char* pathname);

#endif
