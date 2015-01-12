#include "SortAccel.hpp"

#include <cstdio>
#include <stdlib.h>
#include <string.h>

int main() {
    SortAccel* sa = new SortAccel();
    sa->trace("debug.vcd");
    sa->reset();

    if (!sa->test1()) {
        printf("TEST FAIL!\n");
    }
    sa->run(2);
    delete sa;
    return 0;
}

