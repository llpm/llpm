CFLAGS=-O0
CXXFLAGS=${CFLAGS}

CLANGXX=../../../bin/llvm/bin/clang++
CLANG=../../../bin/llvm/bin/clang
CXX=$(CLANGXX)
DIS=../../../bin/llvm/bin/llvm-dis
LINK=../../../bin/llvm/bin/llvm-link

NAME=sieve
ROOT=SieveAccel

default: ${NAME}_test ${ROOT}_simple.pdf

${NAME}.bc: ${NAME}_accel.bc ${NAME}_accel_test.bc
	${LINK} -o $@ $^

${NAME}_accel.bc: ${NAME}_accel.cpp ${NAME}_accel.hpp

obj/${ROOT}.hpp obj/${ROOT}_sw.bc: ${NAME}.bc ../../../bin/cpphdl
	../../../bin/cpphdl ${NAME}.bc obj ${ROOT}

${ROOT}.hpp: obj/${ROOT}.hpp
	cp obj/${ROOT}.hpp .

${NAME}_test.bc: ${ROOT}.hpp

${NAME}_test: ${NAME}_test.bc obj/${ROOT}_sw.bc
	#${CXX} -c -g -emit-llvm ${CXXFLAGS} ${NAME}_test.cpp -o ${NAME}_test.cpp.bc
	${LINK} -o ${NAME}_test_all.bc $^
	${CXX} -O0 -g -o $@ ${NAME}_test_all.bc

%.bc: %.cpp
	${CLANGXX} -c -g -fno-inline -emit-llvm ${CFLAGS} $< -o $@
	${DIS} $@

%.pdf: obj/%.gv
	dot -Tpdf -o $@ $< || true

clean:
	rm -rf ${NAME}_test ${ROOT}.hpp *.bc obj *.ll *.tmp
