#include "file.h"

#include "debug.h"
#include "dir.h"
#include "global.h"
#include "inode.h"
#include "memory.h"
#include "partition_manager.h"
#include "stdint.h"
#include "stdio.h"
#include "stdnull.h"
#include "string.h"
#include "super_block.h"

// Public
void file_table_init(void);

int32_t file_create(struct dir* parent, char* filename);
int32_t file_open(uint32_t inode_no, int32_t flags);
int32_t file_write(int32_t global_fd, const void* buf, int32_t size);

// Private
static int32_t get_global_fd(void);

void file_table_init(void) {
  int32_t i;
  for (i = 0; i < MAX_FILE; i++) {
    file_table[i].inode_elem = NULL;
  }
}

int32_t file_create(struct dir* parent, char* filename) {
  int32_t global_fd = get_global_fd();
  if (global_fd < 0) {
    return -1;
  }

  int32_t inode_no = get_free_inode_no(parent->inode_elem->partmgr);
  if (inode_no < 0) {
    return -1;
  }

  struct inode_elem* inode_elem =
      inode_create(parent->inode_elem->partmgr, inode_no);
  if (inode_elem == NULL) {
    release_inode_no(parent->inode_elem->partmgr, inode_no);
    return -1;
  }

  struct dir_entry de;
  de.f_type = TYPE_NORMAL;
  de.inode_no = inode_no;
  strcpy(de.filename, filename);

  if (dir_create_entry(parent, &de) < 0) {
    inode_close(inode_elem);
    release_inode_no(parent->inode_elem->partmgr, inode_no);
    return -1;
  }

  file_table[global_fd].fd_flag = 0;
  file_table[global_fd].fd_pos = 0;
  file_table[global_fd].inode_elem = inode_elem;

  sync_inode_no(parent->inode_elem->partmgr, inode_no);
  inode_sync(inode_elem);

  return global_fd;
}

int32_t file_open(uint32_t inode_no, int32_t flags) {
  int32_t global_fd = get_global_fd();
  if (global_fd < 0) {
    printf("use all global fd\n");
    return -1;
  }

  file_table[global_fd].inode_elem = inode_open(&cur_partition, inode_no);
  if (file_table[global_fd].inode_elem == NULL) {
    return -1;
  }

  // FIXME: add support for only one process can write file at the same time
  file_table[global_fd].fd_flag = flags;
  file_table[global_fd].fd_pos = 0;

  return global_fd;
}

static int32_t get_global_fd() {
  int32_t i;
  // 0, 1, 2 is stdin, stdout and stderr
  for (i = 3; i < MAX_FILE; i++) {
    if (file_table[i].inode_elem == NULL) {
      return i;
    }
  }
  return -1;
}

int32_t file_write(int32_t global_fd, const void* buf, int32_t size) {
  struct file* f = &file_table[global_fd];
  ASSERT(f != NULL);

  uint32_t block_used = inode_block_used(f->inode_elem);
  uint32_t size_all = block_used * BLOCK_SIZE;
  uint32_t size_used = f->inode_elem->inode.size;
  uint32_t size_left = size_all - size_used;

  uint32_t size_after = f->fd_pos + size;
  if (size_after < size_used) {
    size_after = size_used;
  }

  uint32_t block_need = 0;
  if (size_all < size_after) {
    uint32_t size_need = size_after - size_left;
    block_need = DIV_ROUND_UP(size_need, BLOCK_SIZE);
  }

  if (block_need > 0) {
    if (inode_get_blocks(f->inode_elem, block_need) < 0) {
      printf("cannot get new %d blocks\n", block_need);
      return -1;
    }
  }

  char* content = (char*)sys_malloc(BLOCK_SIZE);

  if (inode_read(f->inode_elem, block_used - 1, content) < 0) {
    PANIC("inode read");
  }

  uint32_t off = f->fd_pos % BLOCK_SIZE;
  memcpy(content + off, buf, size);

  if (inode_write(f->inode_elem, block_used - 1, content) < 0) {
    PANIC("inode write");
  }

  f->fd_pos += size;

  sys_free(content);
  return size;
}
