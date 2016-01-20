#!/bin/bash

export CC=gcc
export CXX=g++

mkdir -p bin/flopc++
cd bin/flopc++
insdir=`pwd`
cd ../../
cd ext/
tar -zxf FlopC++-1.2.4.tgz
cd FlopC++-1.2.4
./configure --prefix=$insdir
make 
make install
cd ..
rm -rf FlopC++-1.2.4
cd ..

