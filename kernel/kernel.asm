[bits 32]
%define ERROR_CODE nop      ; if interrupt push error code
%define ZERO push 0         ; no error code, push 0

extern put_str

section .data
intr_str db "interrupt occur!", 0xa, 0
global intr_entry_table
intr_entry_table:

%macro VECTOR 2
section .text
intr_%1_entry:
  %2
  push intr_str
  call put_str
  add esp, 4

  ; send 0x20(EOI) to master and slave
  mov al, 0x20
  out 0xa0, al
  out 0x20, al

  add esp, 4      ; skip error code
  iret

section .data
  dd intr_%1_entry

%endmacro

VECTOR 0x00, ZERO
VECTOR 0x01, ZERO
VECTOR 0x02, ZERO
VECTOR 0x03, ZERO
VECTOR 0x04, ZERO
VECTOR 0x05, ZERO
VECTOR 0x06, ZERO
VECTOR 0x07, ZERO
VECTOR 0x08, ZERO
VECTOR 0x09, ZERO
VECTOR 0x0A, ZERO
VECTOR 0x0B, ZERO
VECTOR 0x0C, ZERO
VECTOR 0x0D, ZERO
VECTOR 0x0E, ZERO
VECTOR 0x0F, ZERO
VECTOR 0x10, ZERO
VECTOR 0x11, ZERO
VECTOR 0x12, ZERO
VECTOR 0x13, ZERO
VECTOR 0x14, ZERO
VECTOR 0x15, ZERO
VECTOR 0x16, ZERO
VECTOR 0x17, ZERO
VECTOR 0x18, ZERO
VECTOR 0x19, ZERO
VECTOR 0x1A, ZERO
VECTOR 0x1B, ZERO
VECTOR 0x1C, ZERO
VECTOR 0x1D, ZERO
VECTOR 0x1E, ERROR_CODE
VECTOR 0x1F, ZERO
VECTOR 0x20, ZERO

