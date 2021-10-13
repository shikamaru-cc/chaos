/*
 * Inline assembly code for IO operation
 * Refer to the book for more information
 *
 * Machine mode notation:
 * b -- take lowest byte of regs
 *      example: [a-d]l
 * w -- take lowest 2 byte of regs
 *      example: [a-d]x
 */

#ifndef __LIB_KERNEL_IO_H
#define __LIB_KERNEL_IO_H
#include "stdint.h"

// out one byte to given port
static inline void outb(uint16_t port, uint8_t data) {
  asm volatile("outb %b0, %w1" : : "a"(data), "Nd"(port));
}

// out word_count words to port
static inline void outsw(uint16_t port, const void* addr, uint32_t word_cnt) {
  asm volatile("cld; rep outsw" : "+S"(addr), "+c"(word_cnt) : "d"(port));
}

// in one byte from port
static inline uint8_t inb(uint16_t port) {
  uint8_t data;
  asm volatile("inb %w1, %b0" : "=a"(data) : "Nd"(port));
  return data;
}

// in word_cnt words from port
static inline void insw(uint16_t port, void* addr, uint32_t word_cnt) {
  asm volatile("cld; rep insw"
               : "+D"(addr), "+c"(word_cnt)
               : "d"(port)
               : "memory");
}

#endif
