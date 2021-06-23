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

/* intr_entry_table is the entry point of interrupt, define in kernel.asm, it
 * will call the actual handler in intr_handler_table */
extern intr_handler intr_entry_table[IDT_DESC_CNT];

/* intr_handler_table is the actual handler of interrupt */
intr_handler intr_handler_table[IDT_DESC_CNT];
/* interrupt name */
char* intr_name[IDT_DESC_CNT];

static void general_intr_handler(uint8_t intr_n) {
  if (intr_n == 0x27 || intr_n == 0x2f) {
    // IRQ7 and IRQ15 produce spurious interrupt
    return;
  }
  put_str("int ");
  put_int(intr_n);
  put_str(" : ");
  put_str(intr_name[intr_n]);
  put_char('\n');
}

static void make_idt_desc(struct gate_desc* p_gdesc, 
                          uint8_t attr, intr_handler fn) {
  p_gdesc->func_offset_low_word = (uint32_t)fn & 0x0000FFFF;
  p_gdesc->selector = SELECTOR_K_CODE;
  p_gdesc->dcount = 0;
  p_gdesc->attribute = attr;
  p_gdesc->func_offset_high_word = ((uint32_t)fn & 0xFFFF0000) >> 16;
}

static void exception_init(void) {
  int i;
  for (i = 0; i < IDT_DESC_CNT; i++) {
    // In kernel/kernel.asm, intr_i_entry will call [intr_handler_table+i*4]
    intr_handler_table[i] = general_intr_handler;
    intr_name[i] = "Unknown";
  }
  // initial interrupt name
  intr_name[ 0] = "Division by zero";
  intr_name[ 1] = "Single-step interrupt (see trap flag)";
  intr_name[ 2] = "NMI";
  intr_name[ 3] = "Breakpoint (0xCC encoding of INT3)";
  intr_name[ 4] = "Overflow";
  intr_name[ 5] = "Bound Range Exceeded";
  intr_name[ 6] = "Invalid Opcode";
  intr_name[ 7] = "Coprocessor not available";
  intr_name[ 8] = "Double Fault";
  intr_name[ 9] = "Coprocessor Segment Overrun (386 or earlier only)";
  intr_name[10] = "Invalid Task State Segment";
  intr_name[11] = "Segment not present";
  intr_name[12] = "Stack Segment Fault";
  intr_name[13] = "General Protection Fault";
  intr_name[14] = "Page Fault";
  intr_name[15] = "reserved";
  intr_name[16] = "x87 Floating Point Exception";
  intr_name[17] = "Alignment Check";
  intr_name[18] = "Machine Check";
  intr_name[19] = "SIMD Floating-Point Exception";
  /* TODO: Just for test, remove it */
  intr_name[32] = "Timer interrupt";
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
  exception_init();
  idt_desc_init();  
  pic_init();

  /* load idt */
  uint64_t idt_operand = ((sizeof(idt) - 1)|((uint64_t)((uint32_t)idt << 16)));
  asm volatile("lidt %0": : "m" (idt_operand));
  put_str("idt_init done\n");
}
