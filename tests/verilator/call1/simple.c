#include <stdint.h>

typedef uint64_t Data __attribute__((__vector_size__(64)));

uint64_t sum(Data d) {
    uint64_t total = 0;
    for (unsigned i=0; i<8; i++) {
        total += d[i];
    }
    return total;
}

uint64_t simple(uint64_t a, uint64_t b) {
    Data d = { 
        a, b, a, b,
        a, b, a, b
    };
    return sum(d);
}
