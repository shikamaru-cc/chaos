ENTRY_POINT = 0xc0001500
BINS = boot/mbr.bin boot/loader.bin kernel/kernel.bin

LIB_K_OBJS = $(subst .asm,.o,$(wildcard lib/kernel/*.asm))
D_OBJS = $(subst .c,.o,$(wildcard device/*.c))
OBJS = $(subst .c,.o,$(wildcard kernel/*.c)) \
       $(subst .asm,.o,$(wildcard kernel/*.asm)) \
       $(D_OBJS) \
       $(LIB_K_OBJS)

AS = nasm
CC = build/i386-elf-gcc/bin/i386-elf-gcc
LD = build/i386-elf-gcc/bin/i386-elf-ld
LIB = -I lib/ -I kernel/ -I device/
ASFLAGS = -f elf
CFLAGS = -Wall $(LIB) -c -fno-builtin -W -Wstrict-prototypes \
         -Wmissing-prototypes -g -D DEBUG
LDFLAGS = -Ttext $(ENTRY_POINT) -e main

all: build

# We need to install i386 tools for cross compile
build/init.mk:
	@echo "fetching i386 elf tools"
	mkdir build
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

WORKSPACE/disk.img:
	mkdir WORKSPACE
	bximage -mode=create -hd=10M -q ./WORKSPACE/disk.img

.PHONY: build
build: build/init.mk $(BINS) WORKSPACE/disk.img
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
.PHONY: bochs
# TODO: build/bochs/bochs has some bug in set breakpoints
# change to another version
bochs: build
	bochs -q -f tools/bochsrc
bochs-gdb: build
	$(BOCHS) -q -f tools/gdb-bochsrc

.PHONY: clean
clean:
	rm -rf WORKSPACE
	cd boot && rm -f *.bin
	cd kernel && rm -f *.bin *.o *.d
	cd device && rm -f *.o
	cd lib && rm -f *.o
	cd lib/kernel && rm -f *.o
	cd lib/user && rm -f *.o
