.PHONY: build
build:
	bximage -mode=create -hd=10M -q ./WORKSPACE/disk.img
	nasm -I ./boot/include -o ./boot/mbr.bin ./boot/mbr.S
	dd if=./boot/mbr.bin of=./WORKSPACE/disk.img bs=512 count=1 conv=notrunc
	nasm -I ./boot/include -o ./boot/loader.bin ./boot/loader.S
	dd if=./boot/loader.bin of=./WORKSPACE/disk.img bs=512 count=4 \
	seek=2 conv=notrunc

.PHONY: bochs
bochs: build
	bochs -f ./WORKSPACE/bochsrc

.PHONY: clean
clean:
	rm ./WORKSPACE/disk.img
	cd boot && rm *.bin
