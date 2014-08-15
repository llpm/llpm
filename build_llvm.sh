#!/bin/bash

export CC=clang
export CXX=clang++

mkdir -p bin/llvm
cd bin/llvm
insdir=`pwd`
cd ../../
mkdir -p llvm_build
cd llvm_build
../llvm/configure --prefix=$insdir --enable-libcpp --enable-cxx11 --enable-keep-symbols --enable-jit 
make -j16
make install
cd ..
rm -rf llvm_build
