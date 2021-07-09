#include "debug.h"
#include "memory.h"
#include "stdint.h"
#include "stdnull.h"
#include "string.h"
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

/* Macros to find PDE and PTE index for a given address */
#define PDE_IDX(addr) (addr >> 22)
#define PTE_IDX(addr) ((addr << 10) >> 22)

struct pool {
  struct bitmap pool_bitmap;
  uint32_t phy_addr_start;
  uint32_t pool_size;
};

struct pool kernel_pool, user_pool;

struct virtual_addr kernel_vaddr;

/* Get pg_cnt continuous virtual pages, return the start va for pages for ok,
 * NULL for false */
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
  int bit_start_idx;
  uint32_t cnt = 0;
  uint32_t vaddr_start;
  if (pf == PF_KERNEL) {
    /* Search bitmap for available pages */
    bit_start_idx = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
    if (bit_start_idx < 0) {
      return NULL;
    }
    /* Set vaddr bitmap */
    while (cnt < pg_cnt) {
      bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_start_idx + cnt);
      cnt++;
    }
    vaddr_start = kernel_vaddr.vaddr_start + bit_start_idx * PG_SIZE;
  } else {
    /* TODO: get user vaddr */
  }

  return (void*)vaddr_start;
}

/* pte_ptr
 * Methods to construct the PTE virtual address for a given virtual, use it to
 * modify PTE.
 * 0xffc00000 : set the first 10 bit to 1, point to PDE itself
 * PDE_IDX(vaddr) << 12 : set the second 10 bit as PDE index of virtual
 * address, use to search PTE, then we map out virtual address to physical
 * address of PTE.
 * (PTE_IDX(vaddr) * 4) : the last 12 bit offset in PTE 
 */
uint32_t* pte_ptr(uint32_t vaddr) {
  return (uint32_t*)(0xffc00000 + (PDE_IDX(vaddr) << 12) +
            (PTE_IDX(vaddr) * 4));
}

/* pde_ptr
 * Methods to construct the PDE virtual address for a given virtual, use it to
 * modify PDE.
 * 0xfffff000 : set the first 20 bit to 1, then we map our virtual address to
 * physical address of PDE.
 * PDE_IDX(vaddr) * 4 : the last 12 bit offset in PDE
 */
uint32_t* pde_ptr(uint32_t vaddr) {
  return (uint32_t*)(0xfffff000 + (PDE_IDX(vaddr) * 4));
}

/* Allocate one page in m_pool, return the address */
static void* palloc(struct pool* m_pool) {
  int bit_idx;
  bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1);
  if (bit_idx < 0) {
    return NULL;
  }
  bitmap_set(&m_pool->pool_bitmap, bit_idx);
  return (void*)(m_pool->phy_addr_start + bit_idx * PG_SIZE);
}

/* Add map of given _vaddr and _page_phyaddr to page table */
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
  uint32_t vaddr = (uint32_t)_vaddr;
  uint32_t page_phyaddr = (uint32_t)_page_phyaddr;
  uint32_t* pde = pde_ptr(vaddr);
  uint32_t* pte = pte_ptr(vaddr);
  /* Create PDE if not exist */
  if (!(*pde & PG_P_1)) {
    uint32_t* pde_phyaddr = palloc(&kernel_pool);
    *pde = ((uint32_t)pde_phyaddr | PG_P_1 | PG_RW_W | PG_US_U);
    /* Clean the new page table */
    memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
  }
  /* Create PTE if not exist */
  if (!(*pte & PG_P_1)) {
    *pte = ((uint32_t)page_phyaddr | PG_P_1 | PG_RW_W | PG_US_U);
  } else {
    PANIC("pte exist!");
  }
}

/* Allocate pg_cnt pages */
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
  /* Allocate virtual space pages */
  void* vaddr_start = vaddr_get(pf, pg_cnt);
  if (vaddr_start == NULL) {
    return NULL;
  }

  /* Allocate physical space pages */
  uint32_t vaddr = (uint32_t)vaddr_start;
  struct pool* m_pool = (pf == PF_KERNEL) ? &kernel_pool : &user_pool;
  while (pg_cnt-- > 0) {
    void* phyaddr = palloc(m_pool);
    if (phyaddr == NULL) {
      /* TODO: collect fail allocate memory */
      return NULL;
    }
    /* Map virtual pages and physical pages */
    page_table_add((void*)vaddr, phyaddr);
    vaddr += PG_SIZE;
  }
  return vaddr_start;
}

void* get_kernel_pages(uint32_t pg_cnt) {
  void* vaddr = malloc_page(PF_KERNEL, pg_cnt);
  if (vaddr != NULL) {
    memset(vaddr, 0, pg_cnt * PG_SIZE);
  }
  return vaddr;
}

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
