CFLAGS=-O0
CXXFLAGS=${CFLAGS}
OPTS=-mem2reg -dce -constprop -die -mergereturn -simplifycfg -loop-simplify

CLANGXX=../../../bin/llvm/bin/clang++
CLANG=../../../bin/llvm/bin/clang
CXX=$(CLANGXX)
DIS=../../../bin/llvm/bin/llvm-dis
OPT=../../../bin/llvm/bin/opt

NAME=reg_counter
ROOT=RegCounter

default: ${NAME}_test ${ROOT}.pdf ${ROOT}_simple.pdf



obj/${ROOT}.hpp: ${NAME}.bc ../../../bin/cpphdl
	../../../bin/cpphdl ${NAME}.bc obj ${ROOT}

${ROOT}.hpp: obj/${ROOT}.hpp
	cp obj/${ROOT}.hpp .

${NAME}_test: ${NAME}_test.cpp ${ROOT}.hpp
	${CXX} -c -emit-llvm ${CXXFLAGS} ${NAME}_test.cpp -o ${NAME}_test.cpp.bc
	${CXX} -o ${NAME}_test ${NAME}_test.cpp.bc obj/${ROOT}_verilator.bc

%.bc: %.cpp
	${CLANGXX} -c -emit-llvm ${CFLAGS} $< -o $@.tmp
	${OPT} ${OPTS} $@.tmp -o $@
	${DIS} $@

%.pdf: obj/%.gv
	dot -Tpdf -o $@ $< || true

clean:
	rm -rf ${NAME}_test ${NAME}.hpp *.bc obj *.ll
