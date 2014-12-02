#include "simple.hpp"

#include <stdio.h>

long simple_sw(long a, long b) {
    long i;
    for (i=0; i<b; i++)
        a += 1;
    return a;
}

int main(void) {
    simple* s = new simple();
    s->trace("debug.vcd");
    s->reset();
    s->x(4, 100);
    long l;
    s->a((uint64_t*)&l);
    printf("Result: %lx\n", l);
    printf("S/W Result: %lx\n", simple_sw(4, 100));
    printf("Cycle count: %lu\n", s->cycles());
    s->run(5);
    // s->run(200);
    delete s;
    return 0;
}
