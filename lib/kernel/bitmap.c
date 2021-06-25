#include "bitmap.h"
#include "string.h"

void bitmap_init(struct bitmap* btmp) {
  memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

/* Test if the bits[bit_idx] is set, return true for set */
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx) {
  uint32_t byte_idx = bit_idx / 8;
  uint32_t bit_odd = bit_idx % 8;
  return btmp->bits[byte_idx] & (BITMAP_MASK << bit_odd);
}

/* Scan the bitmap, find whether there are cnt continuous available bits. If yes
 * return the start index, else return -1 */
int bitmap_scan(struct bitmap* btmp, uint32_t cnt) {
  /* skip full used byte */
  uint32_t byte_idx = 0;
  while ((byte_idx < btmp->btmp_bytes_len) && (btmp->bits[byte_idx] == 0xff))
    byte_idx++;

  if (byte_idx >= btmp->btmp_bytes_len) {
    return -1;
  }

  /* count if there are enough available bits */
  uint32_t bit_left = (btmp->btmp_bytes_len - byte_idx) * 8;
  uint32_t bit_p = byte_idx * 8;
  uint32_t count = 0;
  while (bit_left-- > 0) {
    if (bitmap_scan_test(btmp, bit_p)) {
      // bit has been set, recount
      count = 0;
    } else {
      count++;
    }
    if (count == cnt) {
      return bit_p - cnt + 1;
    }
    bit_p++;
  }
  return -1;
}

void bitmap_set(struct bitmap* btmp, uint32_t bit_idx) {
  uint32_t byte_idx = bit_idx / 8;
  uint32_t bit_odd = bit_idx % 8;
  btmp->bits[byte_idx] |= (BITMAP_MASK << bit_odd);
}

void bitmap_unset(struct bitmap* btmp, uint32_t bit_idx) {
  uint32_t byte_idx = bit_idx / 8;
  uint32_t bit_odd = bit_idx % 8;
  btmp->bits[byte_idx] &= ~(BITMAP_MASK << bit_odd);
}
