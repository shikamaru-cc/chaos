;---------- loader and kernel ----------

LOADER_BASE_ADDR equ 0x900
LOADER_START_SECTOR equ 0x2

KERNEL_START_SECTOR equ 9
KERNEL_BIN_BASE_ADDR equ 0x70000
KERNEL_ENTRY_POINT equ 0xc0001500

PAGE_DIR_TABLE_POS equ 0x100000

;---------- gdt descriptor ----------
DESC_G_4K         equ 100000000000000000000000b
DESC_D_32         equ  10000000000000000000000b
DESC_L            equ   0000000000000000000000b
DESC_AVL          equ    000000000000000000000b
DESC_LIMIT_CODE2  equ     11110000000000000000b
DESC_LIMIT_DATA2  equ          DESC_LIMIT_CODE2
DESC_LIMIT_VIDEO2 equ     00000000000000001011b
DESC_P            equ         1000000000000000b
DESC_DPL_0        equ          000000000000000b
DESC_DPL_1        equ          010000000000000b
DESC_DPL_2        equ          100000000000000b
DESC_DPL_3        equ          110000000000000b
DESC_S_CODE       equ            1000000000000b
DESC_S_DATA       equ               DESC_S_CODE
DESC_S_sys        equ            0000000000000b
DESC_TYPE_CODE    equ             100000000000b
DESC_TYPE_DATA    equ             001000000000b

DESC_CODE_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
  DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + \
  DESC_P + DESC_DPL_0 + DESC_S_CODE + \
  DESC_TYPE_CODE + 0x00

DESC_DATA_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
  DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + \
  DESC_P + DESC_DPL_0 + DESC_S_DATA + \
  DESC_TYPE_DATA + 0x00

DESC_VIDEO_HIGH4 equ (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
  DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + \
  DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x00

; ----- selector -----
RPL0    equ    00b
RPL1    equ    01b
RPL2    equ    10b
RPL3    equ    11b
TI_GDT  equ   000b
TI_LDT  equ   100b

; ----- page table property -----
PG_P    equ     1b
PG_RW_R equ    00b
PG_RW_W equ    10b
PG_US_S equ   000b
PG_US_U equ   100b

; ----- program header property -----
PT_NULL equ 0
