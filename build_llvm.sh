#!/bin/bash

export CC=clang
export CXX=clang++

mkdir -p bin/llvm
cd bin/llvm
../../llvm/configure --enable-libcpp --enable-cxx11 --enable-keep-symbols --enable-jit 
cmake ../../llvm
make -j16
cd ../
