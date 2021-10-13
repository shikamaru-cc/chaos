#include "debug.h"

#include "interrupt.h"
#include "kernel/print.h"

void panic_spin(char* filename, int line, const char* fn, const char* cond) {
  intr_disable(); /* disable interrupt */
  put_str("ERROR! in : \n");
  put_str("file : ");
  put_str(filename);
  put_str("\n");
  put_str("line : ");
  put_int(line);
  put_str("\n");
  put_str("function : ");
  put_str((char*)fn);
  put_str("\n");
  put_str("condition : ");
  put_str((char*)cond);
  put_str("\n");
  while (1)
    ; /* stop kernel */
}
