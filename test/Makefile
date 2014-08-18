test.bc: test.c
	clang -c -emit-llvm -O1 test.c
	llvm-dis-3.4 test.bc
