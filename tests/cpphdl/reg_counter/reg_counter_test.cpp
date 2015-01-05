#include "RegCounter.hpp"

#include <cstdio>

int main() {
    char c;
    RegCounter* rc = new RegCounter();
    rc->trace("debug.vcd");
    rc->reset();
    uint64_t start = rc->cycles();

    // Set the initial value
    rc->set_req(0);
    rc->set_resp(&c);
    
    for (unsigned i=0; i<25; i++) {
        // Increment it
        rc->inc_req();
        rc->inc_resp(&c);
        printf("Incremented\n");
    }

    for (unsigned i=0; i<10; i++) {
        // Read it
        uint64_t val = 666;
        rc->read_req();
        rc->read_resp(&val);

        printf("Result: %lu\n", val);
        printf("Cycle count: %lu\n", rc->cycles() - start);
    }
    rc->run(2);
    delete rc;
    return 0;
}
