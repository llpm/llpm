#!/bin/bash

export CC=clang
export CXX=clang++

mkdir -p bin/llvm
cd bin/llvm
insdir=`pwd`
cd ../../
mkdir -p llvm_build
cd llvm_build
../ext/llvm/configure --prefix=$insdir \
    --with-clang-srcdir=../ext/clang --enable-shared \
    --enable-cxx11 --enable-libcpp --enable-jit \
    --enable-debug-runtime --enable-debug-symbols --enable-keep-symbols
make -j16
make install
cd ..
rm -rf llvm_build

