#include "process.h"
#include "memory.h"
#include "thread.h"
#include "global.h"

extern void intr_exit(void);

void process_start(void* filename_) {
  void* function = filename_;
  struct task_struct* cur = running_thread();

  cur->self_kstack += sizeof(struct thread_stack);
  struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;

  proc_stack->edi = proc_stack->esi = proc_stack->ebp = \
  proc_stack->esp_dummy = 0;

  proc_stack->ebx = proc_stack->edx = proc_stack->ecx = \
  proc_stack->eax = 0;

  proc_stack->gs = 0
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
