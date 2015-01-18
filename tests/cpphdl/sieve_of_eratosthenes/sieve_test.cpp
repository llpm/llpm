#include "SieveAccel.hpp"

#include <cstdio>
#include <cassert>
#include <stdlib.h>
#include <string.h>

#include <boost/random.hpp>
#include <limits>

using namespace std;

int main() {
    SieveAccel* sa = new SieveAccel();
    // sa->trace("debug.vcd");
    sa->reset();

    if (!sa->test1()) {
        printf("TEST FAIL!\n");
    } else {
        printf("TEST PASS!\n");
    }

    printf("Running again and timing...\n");
    auto start = sa->cycles();
    sa->findPrimes();
    printf("    cycles: %lu\n", sa->cycles() - start); 

    sa->run(5);
    delete sa;
    return 0;
}

