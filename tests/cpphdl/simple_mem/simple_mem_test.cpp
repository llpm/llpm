#include "SimpleMem.hpp"

#include <cstdio>

#define SIZE 10

int main() {
    char c;
    uint64_t l;

    SimpleMem* mem = new SimpleMem();
    mem->trace("debug.vcd");
    mem->reset();
    uint64_t start = mem->cycles();

    printf("Writing...\n");
    for (unsigned i=0; i<SIZE; i++) {
        mem->write(i, i*2);
    }

    uint64_t sum = 0;
    printf("Reading...\n");
    for (unsigned i=0; i<SIZE; i++) {
        sum += i*2;
        l = mem->read(i);
        printf("Read[%u]: %lu\n", i, l);
        l = mem->readInv(i);
        printf("ReadInv[%u]: %lu\n", i, l);
    }

    printf("R/W Cycles: %lu\n", mem->cycles() - start);

    mem->run(5);

    start = mem->cycles();
    printf("Summing...\n");
    l = mem->sum();
    printf("Sum: %lu\n", l);
    printf("Correct response: %lu\n", sum);
    printf("Sum Cycles: %lu\n", mem->cycles() - start);

    mem->run(2);
    delete mem;
    return 0;
}
