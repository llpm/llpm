CFLAGS=-O0
CXXFLAGS=${CFLAGS}

CLANGXX=../../../bin/llvm/bin/clang++
CLANG=../../../bin/llvm/bin/clang
CXX=$(CLANGXX)
DIS=../../../bin/llvm/bin/llvm-dis
OPT=../../../bin/llvm/bin/opt

NAME=axi_simple
ROOT=AxiSimple

default: ${NAME}_test ${ROOT}_axi.pdf ${ROOT}_axi_simple.pdf

obj/${ROOT}_axi.hpp: ${NAME}.bc ../../../bin/cpphdl
	../../../bin/cpphdl ${NAME}.bc ${ROOT} --wrapper axi --wedge verilator

${ROOT}_axi.hpp: obj/${ROOT}_axi.hpp
	cp obj/${ROOT}_axi.hpp .

${NAME}_test: ${NAME}_test.cpp ${ROOT}_axi.hpp
	${CXX} -c -emit-llvm ${CXXFLAGS} ${NAME}_test.cpp -o ${NAME}_test.cpp.bc
	${CXX} -o ${NAME}_test ${NAME}_test.cpp.bc obj/${ROOT}_axi_sw.bc

%.bc: %.cpp
	${CLANGXX} -c -emit-llvm ${CFLAGS} $< -o $@
	${DIS} $@

%.pdf: obj/%.gv
	dot -Tpdf -o $@ $< || true

clean:
	rm -rf ${NAME}_test ${NAME}.hpp *.bc obj *.ll
