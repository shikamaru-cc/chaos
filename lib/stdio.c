#include "stdio.h"

#include "stdint.h"
#include "stdnull.h"
#include "string.h"
#include "syscall.h"

// --
// Variable argument
// --

typedef char* va_list;

#define va_start(ap, v) (ap = (va_list)&v)
#define va_arg(ap, t) *((t*)(ap += 4))
#define va_end(ap) (ap = NULL)

// --
// Standard io function
// --

// Export methods

uint32_t printf(const char* format, ...);

uint32_t sprintf(char* des, const char* format, ...);

// Non-Export methods

static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base);

uint32_t vsprintf(char* str, const char* format, va_list ap);

// --
// Implementation
// --

static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base) {
  uint32_t m = value % base;
  uint32_t i = value / base;

  if (i) {
    itoa(i, buf_ptr_addr, base);
  }

  if (m < 10) {
    *(*buf_ptr_addr) = m + '0';
  } else {
    *(*buf_ptr_addr) = m - 10 + 'A';
  }

  (*buf_ptr_addr)++;
}

uint32_t vsprintf(char* str, const char* format, va_list ap) {
  char* buf_ptr = str;
  char ch;

  while (ch = *format) {
    if (ch != '%') {
      *buf_ptr = ch;
      buf_ptr++;
      format++;
      continue;
    }

    ch = *(++format);
    int vint;
    char vchar;
    char* vstr;

    switch (ch) {
      case 'x':
        vint = va_arg(ap, int);
        itoa(vint, &buf_ptr, 16);
        break;
      case 'd':
        vint = va_arg(ap, int);
        if (vint < 0) {
          vint = -vint;
          *buf_ptr++ = '-';
        }
        itoa(vint, &buf_ptr, 10);
        break;
      case 'c':
        vchar = va_arg(ap, char);
        *buf_ptr++ = vchar;
        break;
      case 's':
        vstr = va_arg(ap, char*);
        strcpy(buf_ptr, vstr);
        buf_ptr += strlen(buf_ptr);
        break;
    }

    format++;
  }

  *buf_ptr = 0;

  return strlen(str);
}

uint32_t printf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  char buf[1024] = {0};
  vsprintf(buf, format, args);
  va_end(args);
  return write(buf);
}

uint32_t sprintf(char* dst, const char* format, ...) {
  uint32_t ret;
  va_list args;
  va_start(args, format);
  ret = vsprintf(dst, format, args);
  va_end(args);
  return ret;
}
