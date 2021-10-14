#include "thread.h"

#include "debug.h"
#include "interrupt.h"
#include "kernel/list.h"
#include "kernel/print.h"
#include "memory.h"
#include "process.h"
#include "stdint.h"
#include "stdnull.h"
#include "string.h"
#include "sync.h"

// --
// global variable
// --

struct task_struct* main_thread;

struct task_struct* idle_thread;

struct list thread_ready_list;

struct list thread_all_list;

static struct list_elem* thread_tag;

lock_t pid_lock;

// --
// extern definition
// --

extern void switch_to(struct task_struct* cur, struct task_struct* next);

// --
// function prototype
// --

struct task_struct* running_thread();

static void kernel_thread(thread_func* function, void* func_arg);

void thread_create(struct task_struct* pthread, thread_func function,
                   void* func_arg);

pid_t alloc_pid(void);

void task_init(struct task_struct* pthread, char* name, int prio);

struct task_struct* thread_start(char* name, int prio, thread_func function,
                                 void* func_arg);

void thread_block(enum task_status stat);

void thread_unblock(struct task_struct* pthread);

static void make_main_thread(void);

void thread_init(void);

void schedule();

// --
// function implementation
// --

// running_thread
// Get the PCB of current thread. Since each PCB is a single page, the integer
// part of esp is the page virtual address, so the PCB ptr.
struct task_struct* running_thread() {
  uint32_t esp;
  asm volatile("mov %%esp, %0" : "=g"(esp));
  return (struct task_struct*)(esp & 0xfffff000);
}

static void kernel_thread(thread_func* function, void* func_arg) {
  intr_enable();
  function(func_arg);
}

// thread_create
// Setup thread kernel stack. We need to save space for interrupt stack before
// we make progress.
void thread_create(struct task_struct* pthread, thread_func function,
                   void* func_arg) {
  // interrupt stack space
  pthread->self_kstack -= sizeof(struct intr_stack);
  // thread stack space
  pthread->self_kstack -= sizeof(struct thread_stack);

  struct thread_stack* k_stack = (struct thread_stack*)pthread->self_kstack;
  k_stack->eip = kernel_thread;
  k_stack->function = function;
  k_stack->func_arg = func_arg;
  // Callee saved registers
  k_stack->ebp = k_stack->ebx = k_stack->edi = k_stack->esi = 0;
}

pid_t alloc_pid(void) {
  static pid_t next_pid = 0;
  lock_acquire(&pid_lock);
  next_pid++;
  lock_release(&pid_lock);
  return next_pid;
}

// task_init
// Init the task process control block.
void task_init(struct task_struct* pthread, char* name, int prio) {
  memset(pthread, 0, sizeof(*pthread));
  if (pthread == main_thread) {
    pthread->status = TASK_RUNNING;
  } else {
    pthread->status = TASK_READY;
  }
  // self_kstack is the stack top in kernel mode
  pthread->self_kstack = (uint32_t)((uint32_t)pthread + PG_SIZE);
  // TODO: each thread has a pid now
  pthread->pid = alloc_pid();
  strcpy(pthread->name, name);
  pthread->priority = prio;
  pthread->ticks = prio;
  pthread->elapsed_ticks = 0;
  pthread->pgdir = NULL;

  // FIXME: fd_table should be shared by process
  // init file descriptors
  pthread->fd_table[0] = 0;  // stdin
  pthread->fd_table[1] = 1;  // stdout
  pthread->fd_table[2] = 2;  // stderr
  // other fds set to -1
  int i;
  for (i = 3; i < MAX_PROC_OPEN_FD; i++) {
    pthread->fd_table[i] = -1;
  }

  pthread->stack_magic = STACK_MAGIC;
}

// thread_start
// Call task_init and thread_create to setup thread PCB and kernel stack, then
// add thread PCB to thread_ready_list and thread_all_list.
struct task_struct* thread_start(char* name, int prio, thread_func function,
                                 void* func_arg) {
  struct task_struct* thread = get_kernel_pages(1);

  task_init(thread, name, prio);
  thread_create(thread, function, func_arg);

  // Add to ready thread list
  ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
  list_append(&thread_ready_list, &thread->general_tag);

  // Add to all thread list
  ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
  list_append(&thread_all_list, &thread->all_list_tag);

  return thread;
}

// thread_block
// This function is called by current thread to block itself, set its status as
// stat.
void thread_block(enum task_status stat) {
  ASSERT((stat == TASK_BLOCKED) || (stat == TASK_WAITING) ||
         (stat == TASK_HANGING));

  enum intr_status old_status = intr_disable();
  struct task_struct* cur_thread = running_thread();
  cur_thread->status = stat;
  schedule();
  intr_set_status(old_status);
}

// thread_unblock
// Wake up a blocked thread for given PCB ptr.
void thread_unblock(struct task_struct* pthread) {
  enum intr_status old_status = intr_disable();
  ASSERT((pthread->status == TASK_BLOCKED) ||
         (pthread->status == TASK_WAITING) ||
         (pthread->status == TASK_HANGING));

  ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));

  if (elem_find(&thread_ready_list, &pthread->general_tag)) {
    PANIC("thread_unblock: blocked thread in ready_list");
  }

  list_push(&thread_ready_list, &pthread->general_tag);
  pthread->status = TASK_READY;

  intr_set_status(old_status);
}

void thread_yield(void) {
  enum intr_status old_status = intr_disable();
  struct task_struct* cur = running_thread();
  ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
  list_append(&thread_ready_list, &cur->general_tag);
  cur->status = TASK_READY;
  schedule();
  intr_set_status(old_status);
}

static void make_main_thread(void) {
  main_thread = running_thread();
  task_init(main_thread, "main", 31);

  ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
  list_append(&thread_all_list, &main_thread->all_list_tag);
}

static void idle(void* UNUSED_ARG) {
  while (1) {
    thread_block(TASK_BLOCKED);
    // Must open interrupt when hlt
    asm volatile("sti; hlt" : : : "memory");
  }
}

void thread_init(void) {
  put_str("thread_init start\n");
  list_init(&thread_ready_list);
  list_init(&thread_all_list);
  lock_init(&pid_lock);
  make_main_thread();
  idle_thread = thread_start("idle", 10, idle, NULL);
  put_str("thread_init done\n");
}

// schedule
// Our main thread scheduler implementing Round-robin scheduling algorithm. This
// scheduler should be called in the timer interrupt handler.
void schedule() {
  ASSERT(intr_get_status() == INTR_OFF);

  struct task_struct* cur = running_thread();
  if (cur->status == TASK_RUNNING) {
    // thread CPU ticks over
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->status = TASK_READY;
    // reset ticks
    cur->ticks = cur->priority;
  } else {
    // Thread is blocked, do nothing
  }

  if (list_empty(&thread_ready_list)) {
    thread_unblock(idle_thread);
  }

  // Pop the top of list node
  thread_tag = NULL;
  thread_tag = list_pop(&thread_ready_list);

  // Get PCB ptr by general_tag
  struct task_struct* next =
      elem2entry(struct task_struct, general_tag, thread_tag);

  next->status = TASK_RUNNING;
  process_activate(next);
  switch_to(cur, next);
}
