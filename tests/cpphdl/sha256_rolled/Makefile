CFLAGS=-O0
CXXFLAGS=${CFLAGS}

CLANGXX=../../../bin/llvm/bin/clang++
CLANG=../../../bin/llvm/bin/clang
CXX=$(CLANGXX)
DIS=../../../bin/llvm/bin/llvm-dis
LINK=../../../bin/llvm/bin/llvm-link
OPT=../../../bin/llvm/bin/opt

NAME=sha256
ROOT=SHA256

default: ${NAME}_test 
# ${ROOT}.pdf ${ROOT}_simple.pdf

sha256.bc: sha256_accel.bc sha256_accel_test.bc
	${LINK} -o $@ $^

sha256_accel.bc: sha256_accel.cpp sha256_accel.hpp

obj/${ROOT}.hpp: ${NAME}.bc ../../../bin/cpphdl
	../../../bin/cpphdl ${NAME}.bc ${ROOT} --clk 250

${ROOT}.hpp: obj/${ROOT}.hpp
	cp obj/${ROOT}.hpp .

${NAME}_test: ${NAME}_test.cpp ${ROOT}.hpp
	${CXX} -c -emit-llvm ${CXXFLAGS} ${NAME}_test.cpp -o ${NAME}_test.cpp.bc
	${CXX} -O0 -o ${NAME}_test ${NAME}_test.cpp.bc obj/${ROOT}_sw.bc

%.bc: %.cpp
	${CLANGXX} -c -fno-inline -emit-llvm ${CFLAGS} $< -o $@
	${DIS} $@

%.pdf: obj/%.gv
	dot -Tpdf -o $@ $< || true

clean:
	rm -rf ${NAME}_test ${NAME}.hpp *.bc obj *.ll *.tmp
