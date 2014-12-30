#include <stdint.h>
#include <cstring>

#define SIZE 64

class SimpleMem {
    uint64_t mem[SIZE];

    SimpleMem() {
        memset(mem, 0, sizeof(mem));
    }

public:
    uint64_t read(size_t idx);
    uint64_t write(size_t idx, uint64_t val);
    uint64_t sum();
};

uint64_t SimpleMem::read(size_t idx) {
    // if (idx >= SIZE)
        // return 0;
    return mem[idx];
}
uint64_t SimpleMem::write(size_t idx, uint64_t val) {
    // if (idx < SIZE) {
        uint64_t old = mem[idx];
        mem[idx] = val;
        return old;
    // }
    // return 0;
}
#if 0
uint64_t SimpleMem::sum() {
    uint64_t s = 0;
    for (unsigned i=0; i<SIZE; i++) {
        s += mem[i];
    }
    return s;
}
#endif

