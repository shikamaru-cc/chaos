#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "memory.h"

#define PG_SIZE 4096

static void kernel_thread(thread_func* function, void* func_arg) {
  function(func_arg);
}

void thread_create(struct task_struct* pthread,
                   thread_func function,
                   void* func_arg) {
  /* interrupt stack space */
  pthread->self_kstack -= sizeof(struct intr_stack);
  /* thread stack space */
  pthread->self_kstack -= sizeof(struct thread_stack);

  struct thread_stack* k_stack = (struct thread_stack*)pthread->self_kstack;
  k_stack->eip = kernel_thread;
  k_stack->function = function;
  k_stack->func_arg = func_arg;
  /* Caller saved registers */
  k_stack->ebp = k_stack->ebx = k_stack->edi = k_stack->esi = 0;
}

void thread_init(struct task_struct* pthread, char* name, int prio) {
  memset(pthread, 0, sizeof(*pthread));
  pthread->self_kstack = (uint32_t)((uint32_t)pthread + PG_SIZE);
  pthread->status = TASK_RUNNING;
  pthread->priority = prio;
  strcpy(pthread->name, name);
  pthread->stack_magic = 0x12345678;
}

struct task_struct* thread_start(char* name,
                                 int prio,
                                 thread_func function,
                                 void* func_arg) {
  struct task_struct* thread = get_kernel_pages(1);
  thread_init(thread, name, prio);
  thread_create(thread, function, func_arg);

  asm volatile(
    /* Pop saved registers */
    "movl %0, %%esp;"
    "pop %%ebp;"
    "pop %%ebx;"
    "pop %%edi;"
    "pop %%esi;"
    "ret"
    :
    : "g" (thread->self_kstack)
    : "memory"
  );

  return thread;
}
