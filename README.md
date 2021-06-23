# CHAOS

Little toy os written in nasm and C. Running under IA-32, use bochs as an
emulator. Follow the elephant book.

## Build

Simple run:

``` shell
% make
```

Then the disk image file will be created at `WORKSPACE/disk.img`.

It would be very slow when first build the project, since we need to fetch and
build i386-elf-tools from Internet.

To run chaos in bochs:

``` shell
% make bochs
```

For using gdb to debug in bochs:

``` shell
% make bochs-gdb
```

then

``` shell
% gdb kernel/kernel.bin
# in gdb
% target remote localhost:1234
``` 

