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

	; create PDE PTE for kernel
	call setup_page

	; save gdt_ptr, reload using new addr later
	sgdt [gdt_ptr]

	mov ebx, [gdt_ptr+2]
	or dword [ebx+0x18+4], 0xc0000000
	add dword [gdt_ptr+2], 0xc0000000
	add esp, 0xc0000000

	mov eax, PAGE_DIR_TABLE_POS
	mov cr3, eax

	mov eax, cr0
	or eax, 0x80000000
	mov cr0, eax

	lgdt [gdt_ptr]

	mov byte [gs:320], 'V'

	jmp $

; create page dir and page ent
setup_page:
	; clear page dir space
	mov ecx, 4096
	mov esi, 0
.clear_page_dir:
	mov byte [PAGE_DIR_TABLE_POS+esi], 0
	inc esi
	loop .clear_page_dir

; create PDE
.create_pde:
	; The first page table is located at the position
	; after pde, namely PAGE_DIR_TABLE_POS+4k(0x1000)
	mov eax, PAGE_DIR_TABLE_POS
	add eax, 0x1000		; eax point to pte 1
	mov ebx, eax		; save pa of 1st pte for
				; create pte below
	or eax, PG_US_U|PG_RW_W|PG_P
	mov [PAGE_DIR_TABLE_POS + 0x0], eax

	; We set the memory space 3G-4G belong to kernel
	; va from 0xc0000000 to 0xffffffff
	; the first 10bit of 0xc0000000 = 0x300
	; offset in PDE = 0x300 * 4(per PDE entry size)
	; = 0xc00 -> map to the first PTE
	mov [PAGE_DIR_TABLE_POS + 0xc00], eax

	; make the last PDE entry point to PDE self
	sub eax, 0x1000
	mov [PAGE_DIR_TABLE_POS+4092], eax

	; create PTE for kernel
	; We put kernel at the lowest 1MB of physical memory
	mov ecx, 256		; 1M mem/4k per page = 256
	mov esi, 0
	mov edx, PG_US_U | PG_RW_W | PG_P
.create_pte:
	mov [ebx+esi*4], edx
	add edx, 4096
	inc esi
	loop .create_pte
; create PDE for other kernel pages
	mov eax, PAGE_DIR_TABLE_POS
	add eax, 0x2000
	or eax, PG_US_U | PG_RW_W | PG_P
	mov ebx, PAGE_DIR_TABLE_POS
	mov ecx, 254
	mov esi, 769
.create_kernel_pde:
	mov [ebx+esi*4], eax
	inc esi
	add eax, 0x1000
	loop .create_kernel_pde
	ret
