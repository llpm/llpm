#include "RegCounter.hpp"

#include <cstdio>

int main() {
    RegCounter* rc = new RegCounter();
    rc->trace("debug.vcd");
    rc->reset();
    uint64_t start = rc->cycles();
    rc->set_req(0, 10);
    char c;
    rc->set_resp(&c);
    uint64_t val = 666;
    rc->read_req(0);
    rc->read_resp(&val);
    printf("Result: %lu\n", val);
    printf("Cycle count: %lu\n", rc->cycles() - start);
    rc->run(2);
    delete rc;
    return 0;
}
