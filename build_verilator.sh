#!/bin/bash

export CC=clang
export CXX=clang++

mkdir -p bin/verilator
cd bin/verilator
insdir=`pwd`
cd ../../
cd ext/verilator
autoconf
cd ../..
mkdir -p verilator_build
cd verilator_build
cp -r ../ext/verilator/* .
./configure --prefix=$insdir
make 
make install
cd ..
rm -rf verilator_build

