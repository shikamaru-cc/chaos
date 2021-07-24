#!/bin/bash
#
# This script is used to install i386-elf-tools for cross compile
# our os.
# Run this file in build directory and sudo
#
# Reference:
# https://steemit.com/esteem/@geyu/10-cross-compiler-i386-elf-gcc 
#
# Requirements: gcc g++ libx11-dev
#

set -euxo pipefail

gmp=gmp-4.3.2
mpfr=mpfr-2.4.2
mpc=mpc-1.0.1

# build gmp

if [ ! -f gmp.mk ]; then
  curl -O http://mirrors.nju.edu.cn/gnu/gmp/$gmp.tar.bz2
  tar xf $gmp.tar.bz2
  pushd $gmp 
  ./configure
  make
  sudo make install
  popd
  touch gmp.mk
fi


# build mpfr

if [ ! -f mpfr.mk ]; then
  curl -O http://mirrors.nju.edu.cn/gnu/mpfr/$mpfr.tar.bz2
  tar xf $mpfr.tar.bz2
  pushd $mpfr 
  ./configure
  make
  sudo make install
  popd
  touch mpfr.mk
fi

# build mpc

if [ ! -f mpc.mk ]; then
  curl -O http://mirrors.nju.edu.cn/gnu/mpc/$mpc.tar.gz
  tar xf $mpc.tar.gz
  pushd $mpc
  ./configure
  make
  sudo make install
  popd
  touch mpc.mk
fi

PREFIX=$(pwd)/i386-elf-gcc
TARGET=i386-elf

# build binutils
binutils=binutils-2.24

if [ ! -f binutils.mk ]; then
  # If the link 404's, look for a more recent version
  curl -O http://mirrors.nju.edu.cn/gnu/binutils/$binutils.tar.gz
  tar xf $binutils.tar.gz

  mkdir binutils-build
  pushd binutils-build && \
  ../binutils-2.24/configure \
    --target=$TARGET \
    --enable-interwork \
    --enable-multilib \
    --disable-nls \
    --disable-werror \
    --prefix=$PREFIX 2>&1 | tee configure.log
  make all install 2>&1 | tee make.log
  popd
  touch binutils.mk
fi

# build gcc-4.9.1

if [ ! -f gcc.mk ]; then
  curl -O http://mirrors.nju.edu.cn/gnu/gcc/gcc-4.9.1/gcc-4.9.1.tar.bz2
  tar xf gcc-4.9.1.tar.bz2

  mkdir gcc-build
  pushd gcc-build
  ../gcc-4.9.1/configure \
    --target=$TARGET \
    --prefix=$PREFIX \
    --disable-nls \
    --disable-libssp \
    --enable-languages=c \
    --without-headers
  make all-gcc 
  # make all-target-libgcc
  make install-gcc 
  # make install-target-libgcc
  popd
  touch gcc.mk
fi

# build bochs with --enable-gdb-stub

bochs_url='https://jaist.dl.sourceforge.net/project/bochs/bochs/2.6.11/bochs-2.6.11.tar.gz'

if [ ! -f bochs.mk ]; then
  curl -O $bochs_url
  tar -xvf bochs-2.6.11.tar.gz
  mkdir bochs
  pushd bochs-2.6.11
  # build bochs-gdb and bochs independtly, since --enable-gdb-stub and
  # --enable-debugger is mutually exclusive
  ./configure --enable-gdb-stub
  make
  cp bochs ../bochs/bochs-gdb
  cp bximage ../bochs/bximage
  make clean
  ./configure --enable-debugger
  make
  cp -r bios ../bochs/
  cp bochs ../bochs/
  popd
  touch bochs.mk
fi
