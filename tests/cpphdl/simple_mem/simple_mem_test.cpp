#include "SimpleMem.hpp"

#include <cstdio>

int main() {
    char c;
    uint64_t l;

    SimpleMem* mem = new SimpleMem();
    mem->trace("debug.vcd");
    mem->reset();
    uint64_t start = mem->cycles();

    printf("Writing...\n");
    mem->write_req(0, 3, 6);
    mem->write_resp(&l);

    mem->write_req(0, 4, 8);
    mem->write_resp(&l);

    printf("Reading...\n");
    mem->read_req(0, 3);
    mem->read_resp(&l);
    printf("Read[3]: %lu\n", l);

    mem->read_req(0, 4);
    mem->read_resp(&l);
    printf("Read[4]: %lu\n", l);

    mem->run(2);
    delete mem;
    return 0;
}
