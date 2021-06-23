#include "global.h"
#include "interrupt.h"
#include "io.h"

#define PIC_M_CTRL  0x20        // Master control port
#define PIC_M_DATA  0x21        // Master data port
#define PIC_S_CTRL  0xa0        // Slave control port
#define PIC_S_DATA  0xa1        // Slave data port

#define PIC_M_ICW1  0x11        // ICW1: edge, cascade, need ICW4
#define PIC_M_ICW2  0x20        // ICW2: init intr num: 0x20
#define PIC_M_ICW3  0x04        // ICW3: IR2 connect to slave
#define PIC_M_ICW4  0x01        // ICW4: 8086 mode, no auto EOI

#define PIC_S_ICW1  0x11        // ICW1: edge, cascade, need ICW4
#define PIC_S_ICW2  0x28        // ICW2: init intr num: 0x28
#define PIC_S_ICW3  0x02        // ICW3: connect to master IR2
#define PIC_S_ICW4  0x01        // ICW4: 8086 mode, no auto EOI

#define IDT_DESC_CNT 0x21       // num of interrupt types
static struct gate_desc idt[IDT_DESC_CNT]; // interrupt descriptor table
extern intr_handler intr_entry_table[IDT_DESC_CNT];

static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler fn) {
  p_gdesc->func_offset_low_word = (uint32_t)fn & 0x0000FFFF;
  p_gdesc->selector = SELECTOR_K_CODE;
  p_gdesc->dcount = 0;
  p_gdesc->attribute = attr;
  p_gdesc->func_offset_high_word = ((uint32_t)fn & 0xFFFF0000) >> 16;
}

static void idt_desc_init(void) {
  put_str("    setup interrupt descriptor table\n");
  int i;
  for (i = 0; i < IDT_DESC_CNT; i++) {
    make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
  }
}

// init pic 8259A
static void pic_init(void) {

  // init master
  outb(PIC_M_CTRL, PIC_M_ICW1);
  outb(PIC_M_DATA, PIC_M_ICW2);
  outb(PIC_M_DATA, PIC_M_ICW3);
  outb(PIC_M_DATA, PIC_M_ICW4);

  // init slave
  outb(PIC_S_CTRL, PIC_S_ICW1);
  outb(PIC_S_DATA, PIC_S_ICW2);
  outb(PIC_S_DATA, PIC_S_ICW3);
  outb(PIC_S_DATA, PIC_S_ICW4);

  // Open IR0
  // Actually we are outing data to OCW1
  outb(PIC_M_DATA, 0xfe);
  outb(PIC_S_DATA, 0xff);

  put_str("    init pic\n");
}

void idt_init(void) {
  put_str("idt_init start\n");
  idt_desc_init();  
  pic_init();

  /* load idt */
  uint64_t idt_operand = ((sizeof(idt) - 1)|((uint64_t)((uint32_t)idt << 16)));
  asm volatile("lidt %0": : "m" (idt_operand));
  put_str("idt_init done\n");
}
