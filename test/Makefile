CFLAGS=-O1
CXXFLAGS=-std=c++1y ${CFLAGS}

ALL = test.bc cpptest.bc

default: ${ALL} 

%.bc: %.c
	clang -c -emit-llvm ${CFLAGS} $< -o $@
	llvm-dis-3.4 $@

%.bc: %.cpp
	clang++ -c -emit-llvm ${CXXFLAGS} $< -o $@
	llvm-dis-3.4 $@

clean:
	rm ${ALL}
