#include "SimpleMem.hpp"

#include <cstdio>

#define SIZE 8

int main() {
    char c;
    uint64_t l;

    SimpleMem* mem = new SimpleMem();
    mem->trace("debug.vcd");
    mem->reset();
    uint64_t start = mem->cycles();

    printf("Writing...\n");
    for (unsigned i=0; i<SIZE; i++) {
        mem->write_req(0, i, i*2);
        mem->write_resp(&l);
    }

    uint64_t sum = 0;
    printf("Reading...\n");
    for (unsigned i=0; i<SIZE; i++) {
        sum += i*2;
        mem->read_req(0, i);
        mem->read_resp(&l);
        printf("Read[%u]: %lu\n", i, l);
#if 0
        mem->readInv_req(0, i);
        mem->readInv_resp(&l);
        printf("ReadInv[%u]: %lu\n", i, l);
#endif
    }

    mem->run(5);

    printf("Summing...\n");
    mem->sum_req(0);
    mem->sum_resp(&l);
    printf("Sum: %lu\n", l);
    printf("Correct response: %lu\n", sum);

    mem->run(2);
    delete mem;
    return 0;
}
