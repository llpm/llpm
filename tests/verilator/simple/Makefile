CFLAGS=-O1
CXXFLAGS=-std=c++1y ${CFLAGS}
CXX=../../../bin/llvm/bin/clang++

default: simple_test

obj/simple.hpp: simple.bc ../../../bin/llvm2verilog
	../../../bin/llvm2verilog simple.bc obj simple

simple.hpp: obj/simple.hpp
	cp obj/simple.hpp .

simple_test: simple_test.cpp simple.hpp
	${CXX} -c -emit-llvm ${CXXFLAGS} simple_test.cpp -o simple_test.cpp.bc
	${CXX} -o simple_test simple_test.cpp.bc obj/simple_verilator.bc

%.bc: %.c
	clang -c -emit-llvm ${CFLAGS} $< -o $@
	llvm-dis-3.4 $@

%.bc: %.cpp
	clang++ -c -emit-llvm ${CXXFLAGS} $< -o $@
	llvm-dis-3.4 $@

clean:
	rm -rf simple_test simple.hpp *.bc obj *.ll