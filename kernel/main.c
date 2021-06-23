#include "init.h"
#include "print.h"

void main(void){
  init_all();
  put_str("\nWelcome to Chaos ..\n");
  asm volatile("sti");
  while(1);
}
