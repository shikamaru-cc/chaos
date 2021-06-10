%include "boot.inc"
section loader vstart=LOADER_BASE_ADDR
	LOADER_STACK_TOP equ LOADER_BASE_ADDR
	jmp loader_start

[bits 16]
; build gdt and internal descriptor
GDT_BASE:
	dd 0x00000000
	dd 0x00000000

CODE_DESC:
	dd 0x0000FFFF
	dd DESC_CODE_HIGH4

DATA_STACK_DESC:
	dd 0x0000FFFF
	dd DESC_DATA_HIGH4

VIDEO_DESC:
	dd 0x80000007 			;limit = (0xbffff-0xb8000)/4k=0x7
	dd DESC_VIDEO_HIGH4

GDT_SIZE  equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1
times 60 dq 0
SELECTOR_CODE  equ (0x0001<<3) + TI_GDT + RPL0
SELECTOR_DATA  equ (0x0002<<3) + TI_GDT + RPL0
SELECTOR_VIDEO equ (0x0003<<3) + TI_GDT + RPL0

gdt_ptr	dw GDT_LIMIT
	dd GDT_BASE

loadermsg db "chaos continue.."
msglen dw $-loadermsg

loader_start:
	; print messgae
	; int0x10 function no: 0x13
	; input:
	; AH = 13H (function no)
	; BH = page num
	; BL = property (AL = 00H|01H)
	; CX = str len
	; (DH, DL) = pos(row, col)
	; AL = display output mode
	; 	01 -> str only contain char(no property),
	;		cursor pos change
	mov sp, LOADER_BASE_ADDR
	mov bp, loadermsg		; ES:BP = str addr
	mov cx, [msglen]
	mov ax, 0x1301			; AH = 13, AL = 01h
	mov bx, 0x001f			; BH = page num BL = color
	mov dx, 0x1800
	int 0x10

	; ready to get in protect mode
	; 1. open A20
	; 2. load GDT
	; 3. set cr0 pe -> 1

	; --- open A20 ---
	in al, 0x92
	or al, 0000_0010B
	out 0x92, al

	; --- load GDT ---
	lgdt [gdt_ptr]
	; cr0 pos1

	; --- cr0 pos1 ---
	mov eax, cr0
	or eax, 0x00000001
	mov cr0, eax

	jmp dword SELECTOR_CODE:p_mode_start ; flush pipeline

[bits 32]
p_mode_start:
	mov ax, SELECTOR_DATA
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov ss, ax
	mov esp, LOADER_STACK_TOP
	mov ax, SELECTOR_VIDEO
	mov gs, ax

	mov byte [gs:160], 'P'

	jmp $
