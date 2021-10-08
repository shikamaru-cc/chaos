ENTRY_POINT = 0xc0001500
BINS = boot/mbr.bin boot/loader.bin kernel/kernel.bin

LIB_OBJS = $(subst .asm,.o,$(wildcard lib/*.asm)) \
           $(subst .c,.o,$(wildcard lib/*.c))

LIB_U_OBJS = $(subst .asm,.o,$(wildcard lib/user/*.asm)) \
             $(subst .c,.o,$(wildcard lib/user/*.c))

LIB_K_OBJS = $(subst .asm,.o,$(wildcard lib/kernel/*.asm)) \
             $(subst .c,.o,$(wildcard lib/kernel/*.c))

FS_OBJS = $(subst .c,.o,$(wildcard fs/*.c))

D_OBJS = $(subst .c,.o,$(wildcard device/*.c))

U_OBJS = $(subst .c,.o,$(wildcard user/*.c))

K_OBJS = $(subst .c,.o,$(wildcard kernel/*.c)) \
         $(subst .asm,.o,$(wildcard kernel/*.asm))

OBJS = $(K_OBJS) \
       $(U_OBJS) \
       $(D_OBJS) \
       $(FS_OBJS) \
       $(LIB_OBJS) \
       $(LIB_K_OBJS)

AS = nasm
CC = build/i386-elf-gcc/bin/i386-elf-gcc
LD = build/i386-elf-gcc/bin/i386-elf-ld
LIB = -I lib/ -I kernel/ -I device/ -I user/ -I fs/
ASFLAGS = -f elf

DEBUG = 1
CFLAGS = -Wall $(LIB) -c -fno-builtin -W -Wstrict-prototypes \
         -Wmissing-prototypes -g
ifeq ($(DEBUG), 1)
  CFLAGS += -D DEBUG
endif
LDFLAGS = -Ttext $(ENTRY_POINT) -e main

# add i386-elf-gcc library
export LD_LIBRARY_PATH=/usr/local/lib

all: build

.PHONY: fetchtool
fetchtool: build/init.mk

# We need to install i386 tools for cross compile
build/init.mk:
	@echo "fetching i386 elf tools"
	mkdir -p build
	cd build && ../tools/install_i386_tools.sh
	touch $@

# build boot binary
boot/%.bin: boot/%.asm
	$(AS) -I boot/include -o $@ $<

# Build object files
# TODO: add dependency for .h file
%.o: %.c
	$(CC) $(CFLAGS) -o $@ $<
%.o: %.asm
	$(AS) $(ASFLAGS) -o $@ $<

# Link
kernel/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

BXIMAGE=build/bochs/bximage
WORKSPACE/init.mk:
	mkdir -p WORKSPACE
	$(BXIMAGE) -mode=create -hd=10M -q ./WORKSPACE/disk.img
	$(BXIMAGE) -mode=create -hd=80M -q ./WORKSPACE/hd80M.img
	echo "I\n./tools/hd80m.sfdisk\nw" | fdisk ./WORKSPACE/hd80M.img
	touch ./WORKSPACE/init.mk

.PHONY: build
build build-debug: build/init.mk $(BINS) WORKSPACE/init.mk
	@echo "build mbr"
	dd if=./boot/mbr.bin of=./WORKSPACE/disk.img bs=512 count=1 conv=notrunc
	@echo "build loader"
	dd if=./boot/loader.bin of=./WORKSPACE/disk.img bs=512 count=4 \
	seek=2 conv=notrunc
	@echo "build kernel"
	dd if=kernel/kernel.bin of=WORKSPACE/disk.img bs=512 count=200 \
	seek=9 conv=notrunc
	@echo "dump kernel.bin"
	objdump -D kernel/kernel.bin > kernel/kernel.d

BOCHS = build/bochs/bochs
BOCHS_GDB = build/bochs/bochs-gdb
.PHONY: bochs
bochs: build
	$(BOCHS) -q -f tools/bochsrc
bochs-gdb: build
	$(BOCHS_GDB) -q -f tools/gdb-bochsrc

.PHONY: clean
clean:
	rm -rf WORKSPACE
	cd boot && rm -f *.bin
	cd kernel && rm -f *.bin *.o *.d
	cd user && rm -f *.o
	cd device && rm -f *.o
	cd lib && rm -f *.o
	cd lib/kernel && rm -f *.o
	cd lib/user && rm -f *.o
