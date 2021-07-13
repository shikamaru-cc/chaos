#ifndef __KERNEL_THREAD_H
#define __KERNEL_THREAD_H

#include "stdint.h"
#include "memory.h"
#include "kernel/list.h"

#define PG_SIZE 4096
#define STACK_MAGIC 0x12345678

typedef void thread_func(void*);

enum task_status {
  TASK_RUNNING,
  TASK_READY,
  TASK_BLOCKED,
  TASK_WAITING,
  TASK_HANGING,
  TASK_DIED
};

struct intr_stack {
// The interrupt prelude would push these registers
  uint32_t vec_no; // interrupt number
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp_dummy;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  uint32_t gs;
  uint32_t fs;
  uint32_t es;
  uint32_t ds;

// Registers will be put while switching priority level
  uint32_t err_code;
  void (*eip) (void);
  uint32_t cs;
  uint32_t eflags;
  uint32_t esp;
  uint32_t ss;
};

struct thread_stack {
// Callee saved registers
  uint32_t ebp;
  uint32_t ebx;
  uint32_t edi;
  uint32_t esi;

  void (*eip) (thread_func* func, void* func_arg);
  void (*unused_retaddr);

  thread_func* function;
  void* func_arg;
};

// process control block
struct task_struct {
  uint32_t self_kstack; // Each thread has its own kernel stack
  char name[16];
  enum task_status status;

  int priority;
  int ticks; // Ticks running on CPU each time
  uint32_t elapsed_ticks; // Total ticks running on CPU

  struct list_elem general_tag; // Tag in ready thread list
  struct list_elem all_list_tag; // Tag in all thread list

  uint32_t* pgdir; // Virtual address of thread's page directory
  struct va_pool u_va_pool; // User process's own virtual address

  uint32_t stack_magic; // Stack boundary
};

struct task_struct* thread_start(
  char* name,
  int prio,
  thread_func function,
  void* func_arg
);

struct task_struct* running_thread(void);
void thread_init(void);
void thread_block(enum task_status);
void thread_unblock(struct task_struct*);
void schedule(void);

#endif
