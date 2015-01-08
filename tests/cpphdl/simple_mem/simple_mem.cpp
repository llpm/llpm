#include <stdint.h>
#include <cstring>

#define SIZE 10

class SimpleMem {
    uint64_t mem[SIZE];
    char identifyme;

    SimpleMem() {
        memset(mem, 0, sizeof(mem));
    }

public:
    uint64_t read(size_t idx);
    uint64_t readInv(size_t idx);
    void write(size_t idx, uint64_t val);
    uint64_t sum();
};

uint64_t SimpleMem::read(size_t idx) {
    // if (idx >= SIZE)
        // return 0;
    return mem[idx];
}
uint64_t SimpleMem::readInv(size_t idx) {
    // if (idx >= SIZE)
        // return 0;
    return mem[SIZE - idx - 1];
}
void SimpleMem::write(size_t idx, uint64_t val) {
    mem[idx] = val;
}
uint64_t SimpleMem::sum() {
    uint64_t s = 0;
    for (uint64_t i=0; i<SIZE; i++) {
        s += mem[i];
    }
    return s;
}

