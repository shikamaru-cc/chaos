#include "debug.h"
#include "stdnull.h"
#include "memory.h"
#include "disk.h"
#include "partition_manager.h"

// NOTE: fs_alloc_inode_no and fs_free_inode_no only modify inode bitmap in
// memory with no operation with disk.
int32_t fs_alloc_inode_no(struct partition_manager* fsm) {
  int32_t free_inode_no;
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;

  free_inode_no = bitmap_scan(inode_btmp_ptr, 1);
  if (free_inode_no < 0) {
    return -1;
  }

  bitmap_set(inode_btmp_ptr, free_inode_no);
  return free_inode_no;
}

void fs_free_inode_no(struct partition_manager* fsm, int32_t inode_no) {
  // Not allow inode_no == 0, which is the root inode no
  ASSERT(inode_no > 0);
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;
  bitmap_unset(inode_btmp_ptr, inode_no);
}

void fs_sync_inode_no(struct partition_manager* fsm, int32_t inode_no) {
  struct bitmap* inode_btmp_ptr = &fsm->inode_btmp;
  // which block contains this inode no
  int32_t block_off = inode_no / BLOCK_BITS;
  uint32_t btmp_lba = fsm->sblock->inode_btmp_lba + block_off;
  // find corresponding bits block in memory
  uint32_t bytes_off = block_off * BLOCK_SIZE;
  ASSERT(bytes_off < inode_btmp_ptr->btmp_bytes_len);
  void* bits_start = inode_btmp_ptr->bits + bytes_off;
  // sync
  disk_write(fsm->part->hd, bits_start, btmp_lba, 1);
}

int32_t fs_alloc_block_no(struct partition_manager* fsm) {
  int32_t free_block_no;
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;

  free_block_no = bitmap_scan(block_btmp_ptr, 1);
  if (free_block_no < 0) {
    return -1;
  }

  bitmap_set(block_btmp_ptr, free_block_no);
  fs_sync_inode_no(fsm, free_block_no);
  return free_block_no;
}

void fs_free_block_no(struct partition_manager* fsm, int32_t block_no) {
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;
  bitmap_unset(block_btmp_ptr, block_no);
  fs_sync_inode_no(fsm, block_no);
}

void fs_sync_block_no(struct partition_manager* fsm, int32_t block_no) {
  struct bitmap* block_btmp_ptr = &fsm->block_btmp;
  // which block contains this block no
  int32_t block_off = block_no / BLOCK_BITS;
  uint32_t btmp_lba = fsm->sblock->block_btmp_lba + block_off;
  // find corresponding bits block in memory
  uint32_t bytes_off = block_off * BLOCK_SIZE;
  ASSERT(bytes_off < block_btmp_ptr->btmp_bytes_len);
  void* bits_start = block_btmp_ptr->bits + bytes_off;
  // sync
  disk_write(fsm->part->hd, bits_start, btmp_lba, 1);
}
