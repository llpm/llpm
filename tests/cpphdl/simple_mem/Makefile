CFLAGS=-O0
CXXFLAGS=${CFLAGS}

CLANGXX=../../../bin/llvm/bin/clang++
CLANG=../../../bin/llvm/bin/clang
CXX=$(CLANGXX)
DIS=../../../bin/llvm/bin/llvm-dis
OPT=../../../bin/llvm/bin/opt

NAME=simple_mem
ROOT=SimpleMem

default: ${NAME}_test ${ROOT}.pdf ${ROOT}_simple.pdf



obj/${ROOT}.hpp: ${NAME}.bc ../../../bin/cpphdl
	../../../bin/cpphdl ${NAME}.bc obj ${ROOT}

${ROOT}.hpp: obj/${ROOT}.hpp
	cp obj/${ROOT}.hpp .

${NAME}_test: ${NAME}_test.cpp ${ROOT}.hpp
	${CXX} -c -emit-llvm ${CXXFLAGS} ${NAME}_test.cpp -o ${NAME}_test.cpp.bc
	${CXX} -o ${NAME}_test ${NAME}_test.cpp.bc obj/${ROOT}_sw.bc

%.bc: %.cpp
	${CLANGXX} -O2 -c -emit-llvm ${CFLAGS} $< -o $@
	${DIS} $@

%.pdf: obj/%.gv
	dot -Tpdf -o $@ $< || true

clean:
	rm -rf ${NAME}_test ${NAME}.hpp *.bc obj *.ll
