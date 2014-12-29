#include "simple.hpp"

#include <stdio.h>

long simple_sw(long a, long b) {
    long i;
    for (i=0; i<b; i++)
        a += i;
    return a;
}

int main(void) {
    simple* s = new simple();
    s->trace("debug.vcd");
    s->reset();
    uint64_t start = s->cycles();
    s->call(4, 25);
    long l;
    s->ret((uint64_t*)&l);
    printf("Result: %ld\n", l);
    printf("S/W Result: %ld\n", simple_sw(4, 25));
    printf("Cycle count: %lu\n", s->cycles() - start);
    s->run(5);
    // s->run(200);
    delete s;
    return 0;
}
