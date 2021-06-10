%include "boot.inc"
SECTION MBR vstart=0x7c00
	mov ax, cs
	mov ds, ax
	mov es, ax
	mov ss, ax
	mov fs, ax
	mov sp, 0x7c00
	mov ax, 0xb800
	mov gs, ax

; ----------------------------------------------------
; INT 0X10 function no:0x06 desc:roll up screen
; ----------------------------------------------------
; input:
; AH = 0x06 function no
; AL = num of rows to roll up(0 to be all)
; BH = property of rows to be rolled up
; (CL,CH) = (X,Y) pos of left up corner of window
; (DL,DH) = (X,Y) pos of right bottom corner of window
; no return value
; 清屏
;---------------------------------------------------
	mov ax, 0600h
	mov bx, 0700h
	mov cx, 0
	mov dx, 184fh
	int 10h

	; 显示"MBR"
	mov byte [gs:0x00], '1'
	mov byte [gs:0x01], 0xA4

	mov byte [gs:0x02], ' '
	mov byte [gs:0x03], 0xA4

	mov byte [gs:0x04], 'M'
	mov byte [gs:0x05], 0xA4

	mov byte [gs:0x06], 'B'
	mov byte [gs:0x07], 0xA4

	mov byte [gs:0x08], 'A'
	mov byte [gs:0x09], 0xA4

	mov eax, LOADER_START_SECTOR
	mov bx, LOADER_BASE_ADDR

	mov eax, LOADER_START_SECTOR
	mov bx, LOADER_BASE_ADDR
	mov cx, 4
	call rd_disk_m_16

	jmp LOADER_BASE_ADDR		; jump to loader

;; eax = LBA sector no
;; bx = addr to load data
;; cx = num of pages to load
rd_disk_m_16:
	mov esi, eax		; backup eax
	mov di, cx		; backup cx
; read disk:
; step1: set num of sectors to load
	mov dx, 0x1f2
	mov al, cl
	out dx, al		; num of sectors to load

	mov eax, esi		; restore ax
; step2: write LBA to 0x1f3~0x1f6
	mov dx, 0x1f3		; bit 7~0 -> 0x1f3
	out dx, al

	mov cl, 8		; bit 15~8 -> 0x1f4
	shr eax, cl
	mov dx, 0x1f4
	out dx, al

	shr eax, cl		; bit 23~16 -> 0x1f5
	mov dx, 0x1f5
	out dx, al

	shr eax, cl
	and al, 0x0f		; bit 24~27 -> 0x1f6
	or al, 0xe0		; set bit 7~4 to 1110: lba mode
	mov dx, 0x1f6
	out dx, al
; step3: write read instruction(0x20) to 0x1f7
	mov dx, 0x1f7
	mov al, 0x20
	out dx, al
; step4: check disk status
 .not_ready:
	; the same port, out: write instruction,
	; in: read disk status
	nop
	in al, dx
	and al, 0x88		; bit4 = 1 -> disk data ready
				; bit7 = 1 -> disk busy
	cmp al, 0x08
	jnz .not_ready
; step5: load data from port 0x1f0 
	; di = num of sectors to load
	; 1 sector = 512 bytes, each time load 1 word
	; totally load 512/2 * di times
	mov ax, di
	mov dx, 256
	mul dx
	mov cx, ax

	mov dx, 0x1f0
 .go_on_read:
	in ax, dx
	mov [bx], ax
	add bx, 2
	loop .go_on_read
	ret

	message db "MBR RUNNING.."
	times 510-($-$$) db 0
	db 0x55, 0xaa
