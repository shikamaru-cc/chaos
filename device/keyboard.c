#include "keyboard.h"
#include "kernel/print.h"
#include "kernel/io.h"
#include "interrupt.h"
#include "stdint.h"
#include "stdbool.h"
#include "global.h"

#define KBD_BUF_PORT 0x60

/* Control Character */
#define ESC       '\033'
#define TAB       '\t'
#define ENTER     '\r'
#define DELETE    '\177'
#define BACKSPACE '\b'

/* Invisible Control Character */
#define INVISIBLE_CHAR 0
#define CTRL_L      INVISIBLE_CHAR
#define CTRL_R      INVISIBLE_CHAR
#define SHIFT_L     INVISIBLE_CHAR
#define SHIFT_R     INVISIBLE_CHAR
#define ALT_L       INVISIBLE_CHAR
#define ALT_R       INVISIBLE_CHAR
#define CAPS_LOCK   INVISIBLE_CHAR
      
/* Macros for makecode and breakcode of control character */
#define SHIFT_L_MAKE    0x2a
#define SHIFT_L_BREAK   0xaa
#define SHIFT_R_MAKE    0x36
#define SHIFT_R_BREAK   0xb6
#define ALT_L_MAKE      0x38
#define ALT_L_BREAK     0xb8
#define ALT_R_MAKE      0xe038
#define ALT_R_BREAK     0xe0b8
#define CTRL_L_MAKE     0x1d
#define CTRL_L_BREAK    0x9d
#define CTRL_R_MAKE     0xe01d
#define CTRL_R_BREAK    0xe09d
#define CAPS_LOCK_MAKE  0x3a
#define CAPS_LOCK_BREAK 0xba

/* Status variable to check whether responding key is made */
static bool ctrl_status, shift_status, alt_status;
static bool caps_lock_status;
static bool caps_lock_hold; /* Is caps lock holded? */

/* Does scancode begin with 0xe0? */
static bool ext_status;

/* keymap for scancode -> ascii */
static char keymap[] = {
/* 0x00 */  '0',
/* 0x01 */  ESC,
/* 0x02 */  '1',
/* 0x03 */  '2',
/* 0x04 */  '3',
/* 0x05 */  '4',
/* 0x06 */  '5',
/* 0x07 */  '6',
/* 0x08 */  '7',
/* 0x09 */  '8',
/* 0x0a */  '9',
/* 0x0b */  '0',
/* 0x0c */  '-',
/* 0x0d */  '=',
/* 0x0e */  BACKSPACE,
/* 0x0f */  TAB,
/* 0x10 */  'q',
/* 0x11 */  'w',
/* 0x12 */  'e',
/* 0x13 */  'r',
/* 0x14 */  't',
/* 0x15 */  'y',
/* 0x16 */  'u',
/* 0x17 */  'i',
/* 0x18 */  'o',
/* 0x19 */  'p',
/* 0x1a */  '[',
/* 0x1b */  ']',
/* 0x1c */  ENTER,
/* 0x1d */  CTRL_L,
/* 0x1e */  'a',
/* 0x1f */  's',
/* 0x20 */  'd',
/* 0x21 */  'f',
/* 0x22 */  'g',
/* 0x23 */  'h',
/* 0x24 */  'j',
/* 0x25 */  'k',
/* 0x26 */  'l',
/* 0x27 */  ';',
/* 0x28 */  '\'',
/* 0x29 */  '`',
/* 0x2a */  SHIFT_L,
/* 0x2b */  '\\',
/* 0x2c */  'z',
/* 0x2d */  'x',
/* 0x2e */  'c',
/* 0x2f */  'v',
/* 0x30 */  'b',
/* 0x31 */  'n',
/* 0x32 */  'm',
/* 0x33 */  ',',
/* 0x34 */  '.',
/* 0x35 */  '/',
/* 0x36 */  SHIFT_R,
/* 0x37 */  '*',
/* 0x38 */  ALT_L,
/* 0x39 */  ' ',
/* 0x3a */  CAPS_LOCK,
};

static char shift_keymap[] = {
/* 0x00 */  0,
/* 0x01 */  ESC,
/* 0x02 */  '!',
/* 0x03 */  '@',
/* 0x04 */  '#',
/* 0x05 */  '$',
/* 0x06 */  '%',
/* 0x07 */  '^',
/* 0x08 */  '&',
/* 0x09 */  '*',
/* 0x0a */  '(',
/* 0x0b */  ')',
/* 0x0c */  '_',
/* 0x0d */  '+',
/* 0x0e */  BACKSPACE,
/* 0x0f */  TAB,
/* 0x10 */  'Q',
/* 0x11 */  'W',
/* 0x12 */  'E',
/* 0x13 */  'R',
/* 0x14 */  'T',
/* 0x15 */  'Y',
/* 0x16 */  'U',
/* 0x17 */  'I',
/* 0x18 */  'O',
/* 0x19 */  'P',
/* 0x1a */  '{',
/* 0x1b */  '}',
/* 0x1c */  ENTER,
/* 0x1d */  CTRL_L,
/* 0x1e */  'A',
/* 0x1f */  'S',
/* 0x20 */  'D',
/* 0x21 */  'F',
/* 0x22 */  'G',
/* 0x23 */  'H',
/* 0x24 */  'J',
/* 0x25 */  'K',
/* 0x26 */  'L',
/* 0x27 */  ':',
/* 0x28 */  '"',
/* 0x29 */  '~',
/* 0x2a */  SHIFT_L,
/* 0x2b */  '|',
/* 0x2c */  'Z',
/* 0x2d */  'X',
/* 0x2e */  'C',
/* 0x2f */  'V',
/* 0x30 */  'B',
/* 0x31 */  'N',
/* 0x32 */  'M',
/* 0x33 */  '<',
/* 0x34 */  '>',
/* 0x35 */  '?',
/* 0x36 */  SHIFT_R,
/* 0x37 */  '*',
/* 0x38 */  ALT_L,
/* 0x39 */  ' ',
/* 0x3a */  CAPS_LOCK
};

static void intr_keyboard_handler(void) {
  uint8_t scancode = inb(KBD_BUF_PORT);

  if (scancode == 0xe0) {
    ext_status = true;
    return;
  }

  /* Make ext scancode */
  uint16_t ext_scancode = (uint16_t)scancode;

  /* ext_status is set, deal with ext code */
  if (ext_status == true) {
    ext_scancode |= 0xe000;
    ext_status = false;
  }

  /* Handle control scancode */
  switch (ext_scancode) {
  /* SHIFT */
  case SHIFT_L_MAKE:
  case SHIFT_R_MAKE:
    shift_status = true;
    break;
  case SHIFT_L_BREAK:
  case SHIFT_R_BREAK:
    shift_status = false;
    break;

  /* ALT */
  case ALT_L_MAKE:
  case ALT_R_MAKE:
    alt_status = true;
    break;
  case ALT_L_BREAK:
  case ALT_R_BREAK:
    alt_status = false;
    break;

  /* CTRL */
  case CTRL_L_MAKE:
  case CTRL_R_MAKE:
    ctrl_status = true;
    break;
  case CTRL_L_BREAK:
  case CTRL_R_BREAK:
    ctrl_status = false;
    break;

  /* CAPS_LOCK 
   * If the caps_lock_hold is false, the CAPS_LOCK key is not pressed. When we
   * first press it, caps_lock_hold is false, change the caps_lock_status and
   * set caps_lock_hold to true, then the caps_lock_status would not be changed
   * by the consequent sent CAPS_LOCK_MAKE until we receive CAPS_LOCK_BREAK.
   */
  case CAPS_LOCK_MAKE:
    if (!caps_lock_hold) {
      caps_lock_status = !caps_lock_status;
      caps_lock_hold = true;
    }
    break;
  case CAPS_LOCK_BREAK:
    caps_lock_hold = false;
    break;
  }

  /* Transfer scancode to ASCII */
  if (ext_scancode > 0x3a) {
    /* Our keymap only handle scancode <= 0x3a now */
    return;
  }

  char* used_map;

  if (shift_status == true) {
    used_map = shift_keymap;
  } else {
    used_map = keymap;
  }

  char ch = used_map[ext_scancode];

  if (ch == INVISIBLE_CHAR) {
    return;
  }

  if (ch >= 'a' && ch <= 'z') {
    /* CAPS_LOCK is set */
    if (caps_lock_status == true) {
      ch -= 32;
    }
  }

  put_char(ch);

  return;
}

void keyboard_init() {
  put_str("keyboard init start\n");
  register_handler(0x21, intr_keyboard_handler);
  put_str("keyboard init done\n");
}
