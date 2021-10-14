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

#define O_CREATE (1 << 0)

extern int32_t sys_open(const char* pathname, int32_t flags);
extern int32_t sys_write(int32_t fd, const void* buf, int32_t size);

#endif
