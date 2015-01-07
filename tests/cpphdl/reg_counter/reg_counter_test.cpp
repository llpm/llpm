#include "RegCounter.hpp"

#include <cstdio>

int main() {
    RegCounter* rc = new RegCounter();
    rc->trace("debug.vcd");
    rc->reset();
    uint64_t start = rc->cycles();

    // Set the initial value
    rc->set(0);
    
    for (unsigned i=0; i<25; i++) {
        // Increment it
        rc->inc();
        printf("Incremented\n");
    }

    for (unsigned i=0; i<10; i++) {
        // Read it
        uint64_t val = 666;
        if (i % 3 == 0)
            rc->inc();
        val = rc->read();
        printf("Result: %lu\n", val);
        printf("Cycle count: %lu\n", rc->cycles() - start);
    }
    rc->run(2);
    delete rc;
    return 0;
}
