#ifndef __THREAD_H
#define __THREAD_H
#include "stdint.h"

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
  /* The interrupt prelude would push these registers */
  uint32_t vec_no; /* interrupt number */
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
  /* Registers will be put while switching priority level */
  uint32_t err_code;
  void (*eip) (void);
  uint32_t cs;
  uint32_t eflags;
  uint32_t esp;
  uint32_t ss;
};

struct thread_stack {
  /* Caller saved registers */
  uint32_t ebp;
  uint32_t ebx;
  uint32_t edi;
  uint32_t esi;

  void (*eip) (thread_func* func, void* func_arg);

  void (*unused_retaddr);
  thread_func* function;
  void* func_arg;
};

/* process control block */
struct task_struct {
  uint32_t self_kstack;
  enum task_status status;
  int priority;
  char name[16];
  uint32_t stack_magic;
};

struct task_struct* thread_start(char* name,
                                 int prio,
                                 thread_func function,
                                 void* func_arg);
#endif
