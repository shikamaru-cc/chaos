TI_GDT equ 0
RPL0 equ 0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

[bits 32]
section .text

global put_char
put_char:
;; pushad push eax, ecx, edx, original esp, ebp,
;; esi and edi to stack. Total 8 regs
  pushad                          ; store regs
  mov ax, SELECTOR_VIDEO
  mov gs, ax
  
;; use VGA CRT Controller Registers to
;; get cursor position
;; user manual:
;; http://www.osdever.net/FreeVGA/vga/crtcreg.htm
.get_cursor_pos:
  ; write index to addr reg 
  ; 0x0e -> get cursor high
  mov dx, 0x03d4
  mov al, 0x0e
  out dx, al
  ; read data of indexed reg
  ; from port 0x3d5
  mov dx, 0x03d5
  in al, dx
  mov ah, al

  ; 0x0f -> get cursor low
  mov dx, 0x03d4
  mov al, 0x0f
  out dx, al
  mov dx, 0x03d5
  in al, dx
.put_char:
  ; store cursor pos in bx
  mov bx, ax
  ; pushad push 4*8=32 bytes
  ; plus 4 bytes return addr
  ; arg is at esp+36
  mov ecx, [esp+36]

  cmp cl, 0xd
  jz .is_carriage_return

  cmp cl, 0xa
  jz .is_line_feed

  cmp cl, 0x8
  jz .is_backspace

  jmp .put_other

.is_backspace:
  dec bx
  shl bx, 1     ; cursor pos*2 = offset in display mem
  mov byte [gs:bx], 0x20  ; write ' '
  inc bx
  mov byte [gs:bx], 0x07
  shr bx, 1
  jmp .set_cursor

.put_other:
  shl bx, 1
  mov byte [gs:bx], cl    ; ASCII
  inc bx
  mov byte [gs:bx], 0x07  ; char property
  shr bx, 1

  ; set bx is the next pos of cursor  
  ; if cursor pos < 2000, not at the end of
  ; display memory
  inc bx
  cmp bx, 2000
  jl .set_cursor
  ; else
  jmp .roll_screen

.is_line_feed:
.is_carriage_return:
  xor dx, dx
  mov ax, bx
  mov si, 80

  div si
  sub bx, dx
  add bx, 80 
  cmp bx, 2000
  jl .set_cursor
  ; else go to roll screen

.roll_screen:
  cld
  mov edi, 0xc00b8000+0
  mov esi, 0xc00b8000+80*2
  mov ecx, 2000-80
  rep movsw

  mov eax, 1920*2
  mov ecx, 80
.clr_new_line:
  mov word [gs:eax], 0x0720
  add eax, 2
  loop .clr_new_line
  ; move cursor up one line
  sub bx, 80

.set_cursor:
  ; write cursor high
  mov dx, 0x03d4
  mov al, 0x0e
  out dx, al
  mov dx, 0x03d5
  mov al, bh
  out dx, al
  ; write cursor low
  mov dx, 0x03d4
  mov al, 0x0f
  out dx, al
  mov dx, 0x03d5
  mov al, bl
  out dx, al

.putchar_done:
  popad
  ret
