#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H
void panic_spin(char* filename, int line, const char* fn, const char* cond);

#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

#ifdef DEBUG
#define ASSERT(CONDITION) \
if(!(CONDITION)) {          \
  PANIC(#CONDITION);      \
}
#else
#define ASSERT(CONDITION) ((void)0)
#endif

#endif
