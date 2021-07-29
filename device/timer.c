#include "debug.h"
#include "global.h"
#include "stdint.h"
#include "timer.h"
#include "thread.h"
#include "kernel/io.h"
#include "kernel/print.h"

#define IRQ0_FREQUENCY    100      // 100 timer interrupt per second
#define INPUT_FREQUENCY   1193180  // timer device CLK frequency
#define PIT_CONTROL_PORT  0x43
#define COUNTER0_VALUE    INPUT_FREQUENCY / IRQ0_FREQUENCY
#define COUNTER0_PORT     0x40
#define COUNTER0_NO       0
#define COUNTER_MODE      2
#define READ_WRITE_LATCH  3

uint32_t ticks; /* CPU total ticks since boot */

static void intr_timer_handler(void) {
  struct task_struct* cur_thread = running_thread();
  /* Check kernel stack overflow */
  ASSERT(cur_thread->stack_magic == STACK_MAGIC);

  cur_thread->elapsed_ticks++;
  ticks++;

  if (cur_thread->ticks <= 0) {
    schedule();
  } else {
    cur_thread->ticks--;
  }
}

static void timer_set_frequence(uint8_t counter_port, 
                                uint8_t counter_no,
                                uint8_t rwl,
                                uint8_t counter_mode,
                                uint16_t counter_value) {
  outb(PIT_CONTROL_PORT, 
        (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
  /* write low 8 bit */
  outb(counter_port, (uint8_t)counter_value);
  /* write high 8 bit */
  outb(counter_port, (uint8_t)(counter_value >> 8));
}

void ticksleep(uint32_t sleep_ticks) {
  uint32_t start_tick = ticks;
  while (1) {
    if (ticks - start_tick < sleep_ticks) {
      thread_yield();
    }
  }
}

void sys_milisleep(uint32_t miliseconds) {
  uint32_t milisecond_per_intr = 1000 / IRQ0_FREQUENCY;
  uint32_t sleep_ticks = DIV_ROUND_UP(miliseconds, milisecond_per_intr);
  ASSERT(sleep_ticks > 0);
  ticksleep(sleep_ticks);
}

void sys_sleep(uint32_t seconds) {
  uint32_t sleep_ticks = seconds * IRQ0_FREQUENCY;
  ASSERT(sleep_ticks > 0);
  ticksleep(sleep_ticks);
}

void timer_init() {
  timer_set_frequence(COUNTER0_PORT, 
                      COUNTER0_NO,
                      READ_WRITE_LATCH,
                      COUNTER_MODE,
                      COUNTER0_VALUE);    
  register_handler(0x20, intr_timer_handler);
  put_str("timer init done\n");
}
