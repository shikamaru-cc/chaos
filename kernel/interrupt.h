#ifndef __INTERRUPT_H
#define __INTERRUPT_H
#include "stdint.h"
struct gate_desc {
  uint16_t func_offset_low_word;
  uint16_t selector;
  uint8_t dcount;
  uint8_t attribute;
  uint16_t func_offset_high_word;
};

typedef void* intr_handler;

static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler fn);
static void exception_init(void);
static void idt_desc_init(void);
static void pic_init(void);

void idt_init(void);

#endif
