CFLAGS=-O1
CXXFLAGS=${CFLAGS}
CXX=../../../bin/llvm/bin/clang++

default: simple_test simple.pdf

simple.pdf: obj/simple.gv
	dot -Tpdf -o simple.pdf obj/simple.gv

obj/simple.hpp: simple.bc ../../../bin/llvm2verilog
	../../../bin/llvm2verilog simple.bc simple

simple.hpp: obj/simple.hpp
	cp obj/simple.hpp .

simple_test: simple_test.cpp simple.hpp
	${CXX} -c -emit-llvm ${CXXFLAGS} simple_test.cpp -o simple_test.cpp.bc
	${CXX} -o simple_test simple_test.cpp.bc obj/simple_sw.bc

%.bc: %.c
	clang -c -emit-llvm ${CFLAGS} $< -o $@
	llvm-dis $@

%.bc: %.cpp
	clang++ -c -emit-llvm ${CXXFLAGS} $< -o $@
	llvm-dis $@

clean:
	rm -rf simple_test simple.hpp *.bc obj *.ll
