#include "debug.h"
#include "process.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "stdint.h"
#include "stdnull.h"
#include "string.h"
#include "global.h"
#include "kernel/list.h"
#include "kernel/bitmap.h"

#define DEFAULT_PRIO 31

extern void intr_exit(void);

// Set up intr_stack for new created process
void process_start(void* filename_) {
  intr_disable();

  void* function = filename_;
  struct task_struct* cur = running_thread();

  cur->self_kstack += sizeof(struct thread_stack);
  struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;

  proc_stack->edi = \
  proc_stack->esi = \
  proc_stack->ebp = \
  proc_stack->esp_dummy = 0;

  proc_stack->ebx = \
  proc_stack->edx = \
  proc_stack->ecx = \
  proc_stack->eax = 0;

  proc_stack->gs = 0; // not allow user proc to access displayer
  proc_stack->fs = proc_stack->es = proc_stack->ds = SELECTOR_U_DATA;

  proc_stack->eip = function;
  proc_stack->cs = SELECTOR_U_CODE;

  proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);

  proc_stack->esp = \
    (uint32_t)get_a_page(PF_USER, USER_STACK_TOP - PG_SIZE) + PG_SIZE;

  proc_stack->ss = SELECTOR_U_DATA;

  asm volatile (
    "movl %0, %%esp;"
    "jmp intr_exit"
    :
    : "g" (proc_stack)
    : "memory"
  );
}

#define KERNEL_PGDIR_VA 0x100000

void page_dir_activate(struct task_struct* pthread) {
  uint32_t pgdir_pa = KERNEL_PGDIR_VA;

  if (pthread->pgdir != NULL) {
    pgdir_pa = va2pa((uint32_t)pthread->pgdir);
  }

  asm volatile ("movl %0, %%cr3" : : "r" (pgdir_pa) : "memory");
}

void process_activate(struct task_struct* pthread) {
  ASSERT(pthread != NULL);
  page_dir_activate(pthread);

  if (pthread->pgdir) {
    update_tss_esp(pthread);
  }
}

uint32_t* create_page_dir(void) {
  uint32_t* pgdir_va = get_kernel_pages(1);
  if (pgdir_va == NULL) {
    return NULL;
  }

  // copy kernel pde for kernel space (0xc0000000 ~ )
  uint32_t k_base_offset = PDE_IDX(K_BASE_ADDR) * 4;
  uint32_t* dst = (uint32_t*)((uint32_t)pgdir_va + k_base_offset);
  uint32_t* src = (uint32_t*)(0xfffff000 + k_base_offset);
  uint32_t size = PG_SIZE - k_base_offset;
  memcpy(dst, src, size);

  // update physical address for directory page itself
  uint32_t pgdir_pa = va2pa((uint32_t)pgdir_va);
  pgdir_va[1023] = (pgdir_pa | PG_US_U | PG_RW_W | PG_P_1);

  return pgdir_va;
}

void create_user_va_bitmap(struct task_struct* proc) {
  uint32_t bitmap_bytes_len = (K_BASE_ADDR - USER_VADDR_START) / PG_SIZE / 8;
  uint32_t bitmap_pg_cnt = DIV_ROUND_UP(bitmap_bytes_len, PG_SIZE);

  proc->u_va_pool.start = USER_VADDR_START; 
  proc->u_va_pool.btmp.btmp_bytes_len = bitmap_bytes_len;
  proc->u_va_pool.btmp.bits = get_kernel_pages(bitmap_pg_cnt);

  bitmap_init(&proc->u_va_pool.btmp);
}

void process_execute(void* filename, char* name) {
  struct task_struct* pthread = get_kernel_pages(1);
  task_init(pthread, name, DEFAULT_PRIO);
  create_user_va_bitmap(pthread);
  thread_create(pthread, process_start, filename);
  pthread->pgdir = create_page_dir();

  enum intr_status old_status = intr_disable();

  ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
  list_append(&thread_ready_list, &pthread->general_tag);

  ASSERT(!elem_find(&thread_all_list, &pthread->all_list_tag));
  list_append(&thread_all_list, &pthread->all_list_tag);

  intr_set_status(old_status);
}
