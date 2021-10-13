#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H

#include "stdint.h"

// Layout of a disk partition:
// ----------------------------------------------------------------------------
// | bootsec | super block | block bitmap | inode bitmap | inode table | data |
// ----------------------------------------------------------------------------

#define BLOCK_SECS 1  // A block consists of a sector
#define BLOCK_SIZE (BLOCK_SECS * 512)
#define BLOCK_BITS (BLOCK_SECS * 512 * 8)
#define SUPER_BLOCK_MAGIC 0x19970322

struct super_block {
  uint32_t magic;
  uint32_t sec_cnt;
  uint32_t inode_cnt;
  uint32_t part_lba_start;

  uint32_t block_btmp_lba;
  uint32_t block_btmp_secs;

  uint32_t inode_btmp_lba;
  uint32_t inode_btmp_secs;

  uint32_t inode_table_lba;
  uint32_t inode_table_secs;

  uint32_t data_lba;
  uint32_t root_inode_no;
  uint32_t dir_entry_size;

  uint8_t pad[460];
} __attribute__((packed));

#endif
