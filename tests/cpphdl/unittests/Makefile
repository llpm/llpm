CFLAGS=-O0
CXXFLAGS=${CFLAGS}
OPTS=-mem2reg -dce -constprop -die -mergereturn -simplifycfg -loop-simplify

CLANGXX=../../../bin/llvm/bin/clang++
CLANG=../../../bin/llvm/bin/clang
CXX=$(CLANGXX)
DIS=../../../bin/llvm/bin/llvm-dis
OPT=../../../bin/llvm/bin/opt

NAME=basic_tests
ROOT=BasicTests

default: ${NAME}_run ${ROOT}.pdf ${ROOT}_simple.pdf

obj/${ROOT}.hpp: ${NAME}.bc ../../../bin/cpphdl
	../../../bin/cpphdl ${NAME}.bc obj ${ROOT}

${ROOT}.hpp: obj/${ROOT}.hpp
	cp obj/${ROOT}.hpp .

${NAME}_run: ${NAME}_run.cpp ${ROOT}.hpp
	${CXX} -c -emit-llvm ${CXXFLAGS} ${NAME}_run.cpp -o ${NAME}_run.cpp.bc
	${CXX} -o ${NAME}_run ${NAME}_run.cpp.bc obj/${ROOT}_sw.bc

%.bc: %.cpp
	${CLANGXX} -c -emit-llvm ${CFLAGS} $< -o $@.tmp
	${OPT} ${OPTS} $@.tmp -o $@
	${DIS} $@

%.pdf: obj/%.gv
	dot -Tpdf -o $@ $< || true

clean:
	rm -rf ${NAME}_run ${NAME}.hpp *.bc obj *.ll
