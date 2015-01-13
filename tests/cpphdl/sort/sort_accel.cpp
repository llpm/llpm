#include "sort_accel.hpp"

unsigned SortAccel::capacity() {
    return CAPACITY;
}

uint64_t SortAccel::sz() {
    return size;
}

void SortAccel::push(uint64_t v) {
    if (size < CAPACITY)
        vals[size++] = v;
}

uint64_t SortAccel::read(unsigned idx) {
    return vals[idx];
}

void SortAccel::clear(void) {
    size = 0;
}

void SortAccel::sort(void) {
    for (unsigned i = 1; i <= size / 2 + 1; i *= 2)
    {
        for (unsigned j = i; j < size; j += 2 * i)
        {
            Merge(j - i, j, ( (j+i) > size) ? size : (j+i));
        }
    }
}
