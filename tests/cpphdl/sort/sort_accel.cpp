#include "sort_accel.hpp"

unsigned SortAccel::capacity() {
    return CAPACITY;
}

void SortAccel::push(uint64_t v) {
    // if (size < CAPACITY)
        vals[size++] = v;
}

uint64_t SortAccel::read(unsigned idx) {
    return vals[idx];
}

void SortAccel::clear(void) {
    size = 0;
}

void SortAccel::sort(void) {
}
