CFLAGS=-O0
CXXFLAGS=${CFLAGS}
CXX=../../../bin/llvm/bin/clang++
OPTS=-mem2reg -dce -constprop -die -mergereturn -simplifycfg -loop-simplify

default: simple_test simple.pdf

simple.pdf: obj/simple.gv
	dot -Tpdf -o simple.pdf obj/simple.gv

obj/simple.hpp: simple.bc ../../../bin/llvm2verilog
	../../../bin/llvm2verilog simple.bc obj simple

simple.hpp: obj/simple.hpp
	cp obj/simple.hpp .

simple_test: simple_test.cpp simple.hpp
	${CXX} -c -emit-llvm ${CXXFLAGS} simple_test.cpp -o simple_test.cpp.bc
	${CXX} -o simple_test simple_test.cpp.bc obj/simple_verilator.bc

%.bc: %.c
	clang -c -emit-llvm ${CFLAGS} $< -o $@.tmp
	opt ${OPTS} $@.tmp -o $@
	llvm-dis-3.4 $@

%.bc: %.cpp
	clang++ -c -emit-llvm ${CXXFLAGS} $< -o $@
	llvm-dis-3.4 $@

clean:
	rm -rf simple_test simple.hpp *.bc obj *.ll
