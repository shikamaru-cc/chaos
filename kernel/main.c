#include "init.h"
#include "debug.h"
#include "string.h"
#include "interrupt.h"
#include "kernel/print.h"
#include "kernel/bitmap.h"

int main(void) {
  init_all();
  put_str("\nWelcome to Chaos ..\n");
  intr_set_status(INTR_OFF);

  char dst[100], src[100];
  strcpy(src, "kernel test : strcpy 1\n");
  strcpy(dst, "kernel test : strcpy 2\n");
  put_str(src);
  put_str(dst);
  put_int(strlen(src));
  put_char(' ');
  put_int(strcmp(dst, src));
  put_char(*strchr(src, 't'));
  put_char(' ');
  put_char(*strrchr(src, 't'));
  put_str(strcat(dst, src));
  put_int(strchrs(dst, 't'));

  uint32_t len = 4;
  uint8_t bits[len];
  struct bitmap btmp;
  btmp.btmp_bytes_len = len;
  btmp.bits = bits;
  bitmap_init(&btmp);
  bitmap_set(&btmp, 5);
  bitmap_unset(&btmp, 5);
  bitmap_set(&btmp, 8);
  bitmap_set(&btmp, 4);
  bitmap_set(&btmp, 13);
  put_int(bitmap_scan(&btmp, 20));

  while(1);
  return 0;
}
