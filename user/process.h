#ifndef __USER_PROCESS_H
#define __USER_PROCESS_H

// Kernel uses the top 1GB address, then it is the user stack top
#define USER_STACK_TOP (0xc0000000)

// ELF entry point
#define USER_VADDR_START 0x8048000

#endif
