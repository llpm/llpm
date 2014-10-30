#include "simple.hpp"

#include <stdio.h>

int main(void) {
    simple* s = new simple();
    s->trace("debug.vcd");
    s->reset();
    s->x(4);
    uint64_t l;
    s->a(&l);
    printf("Result: %lu\n", l);
    s->run(5);
    delete s;
    return 0;
}
