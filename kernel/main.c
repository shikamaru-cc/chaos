#include "debug.h"
#include "console.h"
#include "init.h"
#include "string.h"
#include "syscall.h"
#include "interrupt.h"
#include "memory.h"
#include "thread.h"
#include "process.h"
#include "stdio.h"
#include "kernel/print.h"

// DEBUG ONLY
#include "rand.h"
#include "disk.h"
#include "stdnull.h"

void u_malloc_test(void);
void disk_test(void* arg);

int main(void) {
  put_str("\nWelcome to Chaos ..\n");
  init_all();

  intr_enable();

  // process_execute(u_malloc_test, "u_malloc_test");
  thread_start("disk_test", 31, disk_test, NULL);

  // while(1);
  thread_block(TASK_BLOCKED);

  return 0;
}

extern struct pa_pool u_pa_pool, k_pa_pool;

void u_malloc_test(void) {
  printf("Test prog for malloc and free PID : %d\n", getpid());

  uint32_t test_cnt = 1024;
  // stack storing allocated address
  uint32_t* s = malloc(sizeof(uint32_t) * test_cnt);

  uint8_t* old_u_bits = malloc(u_pa_pool.btmp.btmp_bytes_len);
  uint8_t* old_k_bits = malloc(k_pa_pool.btmp.btmp_bytes_len);

  uint32_t i;
  for (i = 0; i < u_pa_pool.btmp.btmp_bytes_len; i++) {
    old_u_bits[i] = u_pa_pool.btmp.bits[i];
  }

  for (i = 0; i < k_pa_pool.btmp.btmp_bytes_len; i++) {
    old_k_bits[i] = k_pa_pool.btmp.bits[i];
  }

  void* addr;
  uint32_t size;
  rand_set_seed(1997);
  for (i = 0; i < test_cnt; i++) {
    size = rand();
    addr = malloc(size);
    printf("[test malloc %d] malloc %d bytes beginning at 0x%x\n", i, size, (uint32_t)addr);
    s[i] = (uint32_t)addr;
  }

  for (i = 0; i < test_cnt; i++) {
    printf("[test free %d] free 0x%x\n", i, s[i]);
    free(s[i]);
  }

  for (i = 0; i < u_pa_pool.btmp.btmp_bytes_len; i++) {
    if (old_u_bits[i] != u_pa_pool.btmp.bits[i]) {
      printf("user bits[%d] not match!!\n", i);
      while(1);
    }
  }

  printf("Pass user pa_pool bitmap test.\n");

  while(1);
}

char disk_test_buf_w[512], disk_test_buf_r[512];

void disk_test(void* arg) {
  printf("test disk module\n");
  memset(disk_test_buf_w, 'z', 256);
  memset(disk_test_buf_w + 256, 'x', 255);
  ide_channel_write(&ide_channels[0], 1, 1, DISK_SLAVE, disk_test_buf_w);
  ide_channel_read(&ide_channels[0], 1, 1, DISK_SLAVE, disk_test_buf_r);
  disk_test_buf_r[511] = '\0';
  printf("%s", disk_test_buf_r);

  while(1);
}
