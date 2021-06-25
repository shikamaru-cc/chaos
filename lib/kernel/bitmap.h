#ifndef __LIB_KERNEL_BITMAP_H
#define __LIB_KERNEL_BITMAP_H
#include "stdint.h"
#include "stdbool.h"
#define BITMAP_MASK 1
struct bitmap {
  uint32_t btmp_bytes_len;
  uint8_t* bits; /* pointer point to map */
};

void bitmap_init(struct bitmap* btmp);
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx);
int bitmap_scan(struct bitmap* btmp, uint32_t cnt);
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx);
void bitmap_unset(struct bitmap* btmp, uint32_t bit_idx);
#endif
