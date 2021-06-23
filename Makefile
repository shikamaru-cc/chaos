ld = build/i386-elf-gcc/bin/i386-elf-ld
gcc = build/i386-elf-gcc/bin/i386-elf-gcc

all: build

# We need to install i386 tools for cross compile
build/init.mk:
	@echo "fetching i386 elf tools"
	mkdir build
	cd build && ../tools/install_i386_tools.sh
	touch $@

bins = boot/mbr.bin \
	boot/loader.bin \
	kernel/kernel.bin

boot/%.bin: boot/%.asm
	nasm -I boot/include -o $@ $<

lib_kernel_objs = $(subst .asm,.o,$(wildcard lib/kernel/*.asm))
kernel_objs = $(subst .c,.o,$(wildcard kernel/*.c)) \
	$(subst .asm,.o,$(wildcard kernel/*.asm)) \
	$(lib_kernel_objs)

lib/kernel/%.o: lib/kernel/%.asm
	nasm -f elf -o $@ $<

kernel/%.o: kernel/%.c
	$(gcc) -g -I lib/kernel/ -I lib/ -c -o $@ $<

kernel/%.o: kernel/%.asm
	nasm -f elf -o $@ $<

kernel/kernel.bin: $(kernel_objs)
	$(ld) $^ -Ttext 0xc0001500 -e main -o $@

WORKSPACE/disk.img:
	mkdir WORKSPACE
	bximage -mode=create -hd=10M -q ./WORKSPACE/disk.img

.PHONY: build
build: build/init.mk $(bins) WORKSPACE/disk.img
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

bochs = build/bochs/bochs
.PHONY: bochs
# TODO: build/bochs/bochs has some bug in set breakpoints
# change to another version
bochs: build
	bochs -q -f tools/bochsrc
bochs-gdb: build
	$(bochs) -q -f tools/gdb-bochsrc

.PHONY: clean
clean:
	rm -rf WORKSPACE
	cd boot && rm -f *.bin
	cd kernel && rm -f *.bin *.o *.d
