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

void fs_init(void);

#endif
