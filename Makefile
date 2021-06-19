all: build

# We need to install i386 tools for cross compile
build/init.mk:
	@echo "fetching i386 elf tools"
	mkdir build
	cd build && ../tools/install_i386_tools.sh
	touch $@

bins = 	boot/mbr.bin \
				boot/loader.bin \
				kernel/kernel.bin

kernel_objs = $(subst .c,.o,$(wildcard kernel/*.c))

boot/%.bin: boot/%.asm
	nasm -I ./boot/include -o $@ $<

ld = build/i386-elf-gcc/bin/i386-elf-ld

kernel/kernel.bin: $(kernel_objs)
	$(ld) $< -Ttext 0xc0001500 -e main -o $@

gcc = build/i386-elf-gcc/bin/i386-elf-gcc

%.o:%.c
	$(gcc) -c -o $@ $<

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

.PHONY: bochs
bochs: build
	cd tools && bochs -f bochsrc

.PHONY: clean
clean:
	rm -rf WORKSPACE
	cd boot && rm -f *.bin
	cd kernel && rm -f *.bin *.o
