#include "simple.hpp"

#include <stdio.h>

int main(void) {
    simple* s = new simple();
    s->trace("debug.vcd");
    s->reset();
    uint64_t start = s->cycles();
    uint64_t l = s->call(4, 10);
    printf("Result: %ld\n", l);
    printf("Cycles: %lu\n", s->cycles() - start);
    s->run(5);
    delete s;
    return 0;
}
