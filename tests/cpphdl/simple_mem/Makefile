CFLAGS=-O0
CXXFLAGS=${CFLAGS}
OPTS=-mem2reg -dce -constprop -die -mergereturn -simplifycfg -loop-simplify

CLANGXX=../../../bin/llvm/bin/clang++
CLANG=../../../bin/llvm/bin/clang
CXX=$(CLANGXX)
DIS=../../../bin/llvm/bin/llvm-dis
OPT=../../../bin/llvm/bin/opt

NAME=simple_mem
ROOT=SimpleMem

default: ${NAME}_test ${NAME}.pdf

${ROOT}.pdf: obj/${ROOT}.gv
	dot -Tpdf -o ${ROOT}.pdf obj/${ROOT}.gv

obj/${NAME}.hpp: ${NAME}.bc ../../../bin/llvm2verilog
	../../../bin/cpphdl ${NAME}.bc obj ${ROOT}

${NAME}.hpp: obj/${NAME}.hpp
	cp obj/${NAME}.hpp .

${NAME}_test: ${NAME}_test.cpp ${NAME}.hpp
	${CXX} -c -emit-llvm ${CXXFLAGS} ${NAME}_test.cpp -o ${NAME}_test.cpp.bc
	${CXX} -o ${NAME}_test ${NAME}_test.cpp.bc obj/$(NAME}_verilator.bc

%.bc: %.cpp
	${CLANGXX} -c -emit-llvm ${CFLAGS} $< -o $@.tmp
	${OPT} ${OPTS} $@.tmp -o $@
	${DIS} $@


clean:
	rm -rf ${NAME}_test ${NAME}.hpp *.bc obj *.ll
