#include "debug.h"
#include "memory.h"
#include "sync.h"
#include "stdint.h"
#include "stdnull.h"
#include "stdbool.h"
#include "string.h"
#include "global.h"
#include "thread.h"
#include "kernel/print.h"

#define PG_SIZE 4096

// The memory bitmap base address
// Our stack top at 0xc009f000, kernel PCB at 0xc009e000. We set up 4 page for
// out bitmap, then the bitmap can map total 4096 * 8(bit) * 4(kb) * 4(number of
// bitmap) / 1024 = 512 MB memory.
#define MEM_BITMAP_BASE 0xc009a000

// Kernel heap start address, skip the first 1MB
#define K_HEAP_START 0xc0100000

// The address storing total memory size, defined in boot/loader.asm
#define MEMORY_TOTAL_BYTES_ADDR 0xa00

struct pa_pool {
  lock_t lock;
  struct bitmap btmp;
  uint32_t start;
  uint32_t size;
};

struct pa_pool k_pa_pool, u_pa_pool;

struct va_pool k_va_pool;

static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt);

static void vaddr_free(enum pool_flags pf, void* _vaddr);

uint32_t* pte_ptr(uint32_t vaddr);

uint32_t* pde_ptr(uint32_t vaddr);

static void* palloc(struct pa_pool* m_pool);

static void pfree(struct pa_pool* m_pool, void* _paddr);

static void page_table_add(void* _vaddr, void* _page_phyaddr);

static void page_table_remove(void* _vaddr);

void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);

void free_page(enum pool_flags pf, void* vaddr);

void* get_kernel_pages(uint32_t pg_cnt);

void* get_user_pages(uint32_t pg_cnt);

void* get_a_page(enum pool_flags pf, uint32_t va);

uint32_t va2pa(uint32_t va);

static void mem_pool_init(uint32_t all_mem);

void mem_init();

// -- mem block and descriptor defined in memory.h
// --

// Kernel mem block descriptors
struct mem_block_desc k_block_descs[MEM_BLOCK_DESC_CNT];

// -- Public Method

void mem_block_descs_init(struct mem_block_desc descs[MEM_BLOCK_DESC_CNT]);

// --
// struct arena
// --

struct arena {
  struct mem_block_desc* descptr;
  uint32_t cnt;
  bool large;
};

void* sys_malloc(uint32_t size);

void sys_free(void* vaddr);

// --
// -- Implementation
// --

// Get pg_cnt continuous virtual pages, return the start va for pages for ok,
// NULL for false
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
  int bit_start_idx;
  uint32_t cnt = 0;
  uint32_t vaddr_start;

  if (pf == PF_KERNEL) {
    /* Search bitmap for available pages */
    bit_start_idx = bitmap_scan(&k_va_pool.btmp, pg_cnt);
    if (bit_start_idx < 0) {
      return NULL;
    }
    /* Set vaddr bitmap */
    while (cnt < pg_cnt) {
      bitmap_set(&k_va_pool.btmp, bit_start_idx + cnt);
      cnt++;
    }

    vaddr_start = k_va_pool.start + bit_start_idx * PG_SIZE;

  } else {
    struct task_struct* cur = running_thread();
    bit_start_idx = bitmap_scan(&cur->u_va_pool.btmp, pg_cnt);

    if (bit_start_idx < 0) {
      return NULL;
    }

    while(cnt < pg_cnt) {
      bitmap_set(&cur->u_va_pool.btmp, bit_start_idx + cnt);
      cnt++;
    }

    vaddr_start = cur->u_va_pool.start + bit_start_idx * PG_SIZE;
  }

  return (void*)vaddr_start;
}

static void vaddr_free(enum pool_flags pf, void* _vaddr) {
  struct va_pool* va_pool;
  if (pf == PF_KERNEL) {
    va_pool = &k_va_pool;
  } else {
    struct task_struct* cur = running_thread();
    va_pool = &cur->u_va_pool;
  }

  uint32_t va = (uint32_t)_vaddr;
  ASSERT(((va & 0x00000fff) == 0) && (va >= va_pool->start));
  uint32_t bit_idx = (va - va_pool->start) / PG_SIZE;
  ASSERT(bitmap_scan_test(&va_pool->btmp, bit_idx));
  bitmap_unset(&va_pool->btmp, bit_idx);
}

// pte_ptr
// Methods to construct the PTE virtual address for a given virtual, use it to
// modify PTE.
// 0xffc00000 : set the first 10 bit to 1, point to PDE itself
// PDE_IDX(vaddr) << 12 : set the second 10 bit as PDE index of virtual
// address, use to search PTE, then we map out virtual address to physical
// address of PTE.
// (PTE_IDX(vaddr) * 4) : the last 12 bit offset in PTE
uint32_t* pte_ptr(uint32_t vaddr) {
  return (uint32_t*)(0xffc00000 + (PDE_IDX(vaddr) << 12) +
            (PTE_IDX(vaddr) * 4));
}

// pde_ptr
// Methods to construct the PDE virtual address for a given virtual, use it to
// modify PDE.
// 0xfffff000 : set the first 20 bit to 1, then we map our virtual address to
// physical address of PDE.
// PDE_IDX(vaddr) * 4 : the last 12 bit offset in PDE
uint32_t* pde_ptr(uint32_t vaddr) {
  return (uint32_t*)(0xfffff000 + (PDE_IDX(vaddr) * 4));
}

// Allocate one page in m_pool, return the address
static void* palloc(struct pa_pool* m_pool) {
  lock_acquire(&m_pool->lock);

  int bit_idx;
  bit_idx = bitmap_scan(&m_pool->btmp, 1);
  if (bit_idx < 0) {
    return NULL;
  }
  bitmap_set(&m_pool->btmp, bit_idx);

  lock_release(&m_pool->lock);

  return (void*)(m_pool->start + bit_idx * PG_SIZE);
}

// Free one page in m_pool
static void pfree(struct pa_pool* m_pool, void* _paddr) {
  lock_acquire(&m_pool->lock);

  uint32_t pa = (uint32_t)_paddr;
  ASSERT(((pa & 0x00000fff) == 0) && (pa >= m_pool->start));
  int bit_idx;
  bit_idx = (pa - m_pool->start) / PG_SIZE;
  ASSERT(bitmap_scan_test(&m_pool->btmp, bit_idx));
  bitmap_unset(&m_pool->btmp, bit_idx);

  lock_release(&m_pool->lock);
  return;
}

// Add map of given _vaddr and _page_phyaddr to page table
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
  uint32_t vaddr = (uint32_t)_vaddr;
  uint32_t page_phyaddr = (uint32_t)_page_phyaddr;
  uint32_t* pde = pde_ptr(vaddr);
  uint32_t* pte = pte_ptr(vaddr);

  // Create PDE if not exist
  if (!(*pde & PG_P_1)) {
    uint32_t* pde_phyaddr = palloc(&k_pa_pool);
    *pde = ((uint32_t)pde_phyaddr | PG_P_1 | PG_RW_W | PG_US_U);
    // Clean the new page table
    memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE);
  }

  // Create PTE if not exist
  if (!(*pte & PG_P_1)) {
    *pte = ((uint32_t)page_phyaddr | PG_P_1 | PG_RW_W | PG_US_U);
  } else {
    PANIC("pte exist!");
  }
}

static void page_table_remove(void* _vaddr) {
  uint32_t va = (uint32_t)_vaddr;
  uint32_t* pde = pde_ptr(va);
  uint32_t* pte = pte_ptr(va);

  ASSERT(*pde & PG_P_1);
  ASSERT(*pte & PG_P_1);

  *pte &= ~(PG_P_1);
}

// Allocate pg_cnt pages
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
  // Allocate virtual space pages
  void* vaddr_start = vaddr_get(pf, pg_cnt);
  if (vaddr_start == NULL) {
    return NULL;
  }

  // Allocate physical space pages
  uint32_t vaddr = (uint32_t)vaddr_start;
  struct pa_pool* m_pool = (pf == PF_KERNEL) ? &k_pa_pool : &u_pa_pool;

  while (pg_cnt-- > 0) {
    void* phyaddr = palloc(m_pool);
    if (phyaddr == NULL) {
      // TODO: collect fail allocate memory
      return NULL;
    }
    // Map virtual pages and physical pages
    page_table_add((void*)vaddr, phyaddr);
    vaddr += PG_SIZE;
  }

  return vaddr_start;
}

void free_page(enum pool_flags pf, void* _vaddr) {
  // Remove page table maps
  page_table_remove(_vaddr);

  // Free physical page
  struct pa_pool* m_pool = (pf == PF_KERNEL) ? &k_pa_pool : &u_pa_pool;
  void* paddr = (void*)va2pa((uint32_t)_vaddr);

  if (pf == PF_KERNEL) {
    ASSERT((uint32_t)paddr >= k_pa_pool.start && \
      (uint32_t)paddr < u_pa_pool.start);
  } else {
    ASSERT((uint32_t)paddr >= u_pa_pool.start);
  }

  pfree(m_pool, paddr);

  // Free virtual address
  vaddr_free(pf, _vaddr);
}

// get pg_cnt pages from kernel_pool
void* get_kernel_pages(uint32_t pg_cnt) {
  void* va = malloc_page(PF_KERNEL, pg_cnt);
  if (va != NULL) {
    memset(va, 0, pg_cnt * PG_SIZE);
  }
  return va;
}

// get pg_cnt pages from user_pool
void* get_user_pages(uint32_t pg_cnt) {
  void* va = malloc_page(PF_USER, pg_cnt);
  if (va != NULL) {
    memset(va, 0, pg_cnt * PG_SIZE);
  }
  return va;
}

// get a page from kernel/user pool, map va to it
void* get_a_page(enum pool_flags pf, uint32_t va) {
  struct task_struct* cur = running_thread();

  if (cur->pgdir != NULL && pf == PF_KERNEL) {
    PANIC("get_a_page: not allow user alloc kernel space");
  } else if (cur->pgdir == NULL && pf == PF_USER) {
    PANIC("get_a_page: not allow kernel alloc user space");
  }

  struct pa_pool* pa_pool = pf & PF_KERNEL ? &k_pa_pool : &u_pa_pool;
  struct va_pool* va_pool = pf & PF_KERNEL ? &k_va_pool : &cur->u_va_pool;

  // set va_pool bitmap
  int32_t bit_idx = (va - va_pool->start) / PG_SIZE;
  ASSERT(bit_idx > 0);
  bitmap_set(&va_pool->btmp, bit_idx);

  // alloc physical page
  void* pa = palloc(pa_pool);
  if (pa == NULL) {
    return NULL;
  }

  page_table_add((void*)va, pa);

  return (void*)va;
}

// get physical address for given virtual address
uint32_t va2pa(uint32_t va) {
  uint32_t* pte = pte_ptr(va);
  return ((*pte & 0xfffff000) + (va & 0x00000fff));
}

static void mem_pool_init(uint32_t all_mem) {
  put_str("    mem_pool init start\n");

  // 1. Calculate used memory
  // We created 1 PDE and 255 PTE for kernel in loader, and the least 1MB is
  // used by kernel
  uint32_t page_table_size = PG_SIZE * 256;
  uint32_t used_mem = page_table_size + 0x100000;

  // 2. Allocate pages for kernel and user
  // Simply allocate half of all pages to kernel, the rest to user
  uint32_t free_mem = all_mem - used_mem;
  uint16_t all_free_pages = free_mem / PG_SIZE;
  uint16_t kernel_free_pages = all_free_pages / 2;
  uint16_t user_free_pages = all_free_pages - kernel_free_pages;

  k_pa_pool.size = kernel_free_pages * PG_SIZE;
  u_pa_pool.size = user_free_pages * PG_SIZE;

  uint32_t kp_start = used_mem;
  uint32_t up_start = kernel_free_pages * PG_SIZE + kp_start ;
  k_pa_pool.start = kp_start;
  u_pa_pool.start = up_start;

  // 3. Init kernel and user bitmap
  // TODO: We don't handle the remainder of calculated bitmap length, it may
  // cause loss of memory, but we don't need to check boundary.
  uint32_t kbm_length = kernel_free_pages / 8; // byte length of kernel bitmap
  uint32_t ubm_length = user_free_pages / 8;   // byte length of user bitmap

  k_pa_pool.btmp.btmp_bytes_len = kbm_length;
  u_pa_pool.btmp.btmp_bytes_len = ubm_length;

  k_pa_pool.btmp.bits = (void*)MEM_BITMAP_BASE;
  u_pa_pool.btmp.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

  bitmap_init(&k_pa_pool.btmp);
  bitmap_init(&u_pa_pool.btmp);

  // init lock
  lock_init(&k_pa_pool.lock);
  lock_init(&u_pa_pool.lock);

  put_str("    kernel pool bitmap start : ");
  put_int((int)k_pa_pool.btmp.bits);
  put_char('\n');
  put_str("    kernel pool physical address start : ");
  put_int(k_pa_pool.start);
  put_char('\n');
  put_str("    kernel pool size : ");
  put_int(k_pa_pool.size / 1024 / 1024);
  put_str(" MB\n");

  put_str("    user pool bitmap start : ");
  put_int((int)u_pa_pool.btmp.bits);
  put_char('\n');
  put_str("    user pool physical address start : ");
  put_int(u_pa_pool.start);
  put_char('\n');
  put_str("    user pool size : ");
  put_int(u_pa_pool.size / 1024 / 1024);
  put_str(" MB\n");

  // 4. Init kernel virtual address memory pool
  // Kernel virtual addr pool should be same as physiacl memory pool.
  k_va_pool.btmp.btmp_bytes_len = kbm_length;
  k_va_pool.btmp.bits =
    (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);

  k_va_pool.start = K_HEAP_START;
  bitmap_init(&k_va_pool.btmp);

  put_str("    mem_pool_init done\n");
}

void mem_init() {
  put_str("mem_init start\n");
  uint32_t mem_bytes_total = *(uint32_t*)MEMORY_TOTAL_BYTES_ADDR;

  put_str("total memory : ");
  put_int(mem_bytes_total / 1024 / 1024);
  put_str(" MB\n");

  mem_pool_init(mem_bytes_total);
  mem_block_descs_init(k_block_descs);
  put_str("mem_init done\n");
}

// FIXME: va_pool and mem_block_descs are not thread-safe
void* sys_malloc(uint32_t size) {
  struct task_struct* cur = running_thread();

  struct mem_block_desc* mb_descs;
  mb_descs = (cur->pgdir == NULL) ? k_block_descs : cur->u_block_descs;

  enum pool_flags PF = (cur->pgdir == NULL) ? PF_KERNEL : PF_USER;

  // Size > 1024, allocate large arena
  if (size > 1024) {
    uint32_t pg_cnt = DIV_ROUND_UP((size + sizeof(struct arena)), PG_SIZE);
    struct arena* arena = (struct arena*)malloc_page(PF, pg_cnt);

    if (arena == NULL) {
      return NULL;
    }

    arena->descptr = NULL;
    arena->cnt = pg_cnt;
    arena->large = true;

    return (void*)((uint32_t)arena + sizeof(struct arena));
  }

  // Allocate a free block, return the block address

  uint32_t i;
  struct mem_block_desc* mbd;
  for (i = 0; i < MEM_BLOCK_DESC_CNT; i++) {
    if (size <= mb_descs[i].block_size) {
      mbd = &mb_descs[i];
      break;
    }
  }

  // No free mem block, allocate new arena page
  if (list_empty(&mbd->free_list)) {
    // FIXME: malloc_page is not thread safe
    struct arena* arena = (struct arena*)malloc_page(PF, 1);

    if (arena == NULL) {
      return NULL;
    }

    arena->descptr = mbd;
    arena->cnt = mbd->block_cnt_per_arena;
    arena->large = false;

    // Add free block to descs' free block list
    struct mem_block* block = \
      (struct mem_block*)((uint32_t)arena + sizeof(struct arena));
    for (i = 0; i < arena->cnt; i++) {
      ASSERT((uint32_t)block <= (uint32_t)arena + PG_SIZE - mbd->block_size);
      list_append(&mbd->free_list, &block->free_elem);
      block = (struct mem_block*)((uint32_t)block + mbd->block_size);
    }
  }

  struct mem_block* free_block = \
    elem2entry(struct mem_block, free_elem, list_pop(&mbd->free_list));

  return (void*)free_block;
}

#define block2arena(block_va) ((uint32_t)block_va & 0xfffff000)

void sys_free(void* vaddr) {
  struct task_struct* cur = running_thread();
  enum pool_flags PF = (cur->pgdir == NULL) ? PF_KERNEL : PF_USER;

  struct mem_block* block = (struct mem_block*)vaddr;
  // The block coresponding arena
  struct arena* arena = (struct arena*)block2arena(vaddr);

  // For large arena, free arena pages
  if (arena->large) {
    uint32_t i;
    for (i = 0; i < arena->cnt; i++) {
      free_page(PF, (void*)((uint32_t)arena + i * PG_SIZE));
    }
    return;
  }

  // For general block, just add it to descs' free block list
  struct mem_block_desc* mbd = arena->descptr;
  ASSERT(mbd != NULL);

  ASSERT(!elem_find(&mbd->free_list, &block->free_elem));
  list_push(&mbd->free_list, &block->free_elem);
}

void mem_block_descs_init(struct mem_block_desc descs[MEM_BLOCK_DESC_CNT]) {
  // minimum size is 16 bytes
  uint32_t size = 16;

  int i;
  for (i = 0; i < MEM_BLOCK_DESC_CNT; i++) {
    descs[i].block_size = size;
    descs[i].block_cnt_per_arena = (PG_SIZE - sizeof(struct arena)) / size;

    ASSERT((descs[i].block_size * descs[i].block_cnt_per_arena + \
      sizeof(struct arena)) < PG_SIZE);

    list_init(&descs[i].free_list);
    size *= 2;
  }
}

