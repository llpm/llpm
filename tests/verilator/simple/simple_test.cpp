#include "simple.hpp"

#include <stdio.h>

int main(void) {
    simple* s = new simple();
    s->reset();
    s->x(4, 6);
    uint64_t l;
    s->a(&l);
    printf("Result: %lu\n", l);
    return 0;
}
