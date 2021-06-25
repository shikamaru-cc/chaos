#include "init.h"
#include "debug.h"
#include "print.h"

int main(void) {
  init_all();
  put_str("\nWelcome to Chaos ..\n");
  asm volatile("sti");
  ASSERT(1==2);
  while(1);
  return 0;
}
