#include "memory.h"
#include "stdint.h"
#include "kernel/print.h"

#define PG_SIZE 4096

/* The memory bitmap base address
 * Our stack top at 0xc009f000, kernel PCB at 0xc009e000. We set up 4 page for
 * out bitmap, then the bitmap can map total 4096 * 8(bit) * 4(kb) * 4(number of
 * bitmap) / 1024 = 512 MB memory.
 */
#define MEM_BITMAP_BASE 0xc009a000

/* Kernel heap start address, skip the first 1MB */
#define K_HEAP_START 0xc0100000

/* The address storing total memory size, defined in boot/loader.asm */
#define MEMORY_TOTAL_BYTES_ADDR 0xa00

struct pool {
  struct bitmap pool_bitmap;
  uint32_t phy_addr_start;
  uint32_t pool_size;
};

struct pool kernel_pool, user_pool;

struct virtual_addr kernel_vaddr;

static void mem_pool_init(uint32_t all_mem) {
  put_str("    mem_pool init start\n");

  /* 1. Calculate used memory
   * We created 1 PDE and 255 PTE for kernel in loader, and the least 1MB is 
   * used by kernel 
   */
  uint32_t page_table_size = PG_SIZE * 256;
  uint32_t used_mem = page_table_size + 0x100000;

  /* 2. Allocate pages for kernel and user 
   * Simply allocate half of all pages to kernel, the rest to user
   */  
  uint32_t free_mem = all_mem - used_mem;
  uint16_t all_free_pages = free_mem / PG_SIZE;
  uint16_t kernel_free_pages = all_free_pages / 2;
  uint16_t user_free_pages = all_free_pages - kernel_free_pages;

  kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
  user_pool.pool_size = user_free_pages * PG_SIZE;

  uint32_t kp_start = used_mem;
  uint32_t up_start = kernel_free_pages * PG_SIZE + kp_start ;
  kernel_pool.phy_addr_start = kp_start;
  user_pool.phy_addr_start = up_start;

  /* 3. Init kernel and user bitmap 
   * TODO: We don't handle the remainder of calculated bitmap length, it may 
   * cause loss of memory, but we don't need to check boundary.
   */
  uint32_t kbm_length = kernel_free_pages / 8; // byte length of kernel bitmap
  uint32_t ubm_length = user_free_pages / 8;   // byte length of user bitmap

  kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
  user_pool.pool_bitmap.btmp_bytes_len = ubm_length;

  kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
  user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

  bitmap_init(&kernel_pool.pool_bitmap);
  bitmap_init(&user_pool.pool_bitmap);

  put_str("    kernel pool bitmap start : ");
  put_int((int)kernel_pool.pool_bitmap.bits);
  put_char('\n');
  put_str("    kernel pool physical address start : ");
  put_int(kernel_pool.phy_addr_start);
  put_char('\n');
  put_str("    kernel pool size : ");
  put_int(kernel_pool.pool_size / 1024 / 1024);
  put_str(" MB\n");

  put_str("    user pool bitmap start : ");
  put_int((int)user_pool.pool_bitmap.bits);
  put_char('\n');
  put_str("    user pool physical address start : ");
  put_int(user_pool.phy_addr_start);
  put_char('\n');
  put_str("    user pool size : ");
  put_int(user_pool.pool_size / 1024 / 1024);
  put_str(" MB\n");

  /* 4. Init kernel virtual address memory pool
   * Kernel virtual addr pool should be same as physiacl memory pool.
   */
  kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;
  kernel_vaddr.vaddr_bitmap.bits = 
    (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);

  kernel_vaddr.vaddr_start = K_HEAP_START;
  bitmap_init(&kernel_vaddr.vaddr_bitmap);

  put_str("    mem_pool_init done\n");
}

void mem_init() {
  put_str("mem_init start\n");
  uint32_t mem_bytes_total = *(uint32_t*)MEMORY_TOTAL_BYTES_ADDR;

  put_str("total memory : ");
  put_int(mem_bytes_total / 1024 / 1024);
  put_str(" MB\n");

  mem_pool_init(mem_bytes_total);
  put_str("mem_init done\n");
}
