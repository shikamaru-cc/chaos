#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "stdint.h"
#include "kernel/bitmap.h"

// Kernel base address
#define K_BASE_ADDR 0xc0000000

// Macros to find PDE and PTE index for a given address
#define PDE_IDX(addr) (addr >> 22)
#define PTE_IDX(addr) ((addr << 10) >> 22)

enum pool_flags {
  PF_KERNEL = 1,
  PF_USER = 2
};

// PDE and PTE macros
#define PG_P_0  0
#define PG_P_1  1
#define PG_RW_R 0
#define PG_RW_W (1 << 1)
#define PG_US_S 0
#define PG_US_U (1 << 2)

struct va_pool {
  struct bitmap btmp;
  uint32_t start;
};

extern struct pa_pool k_pa_pool, u_pa_pool;
void* get_kernel_pages(uint32_t pg_cnt);
void* get_user_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf, uint32_t va);
uint32_t va2pa(uint32_t va);
void mem_init(void);

#endif
