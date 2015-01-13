#ifndef __SORT_ACCEL_HPP__
#define __SORT_ACCEL_HPP__

#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <limits>
#include <boost/random.hpp>

#define CAPACITY 256

class SortAccel {
public:
    uint64_t size;
    uint64_t vals[CAPACITY];

    SortAccel() {
    }

    unsigned capacity();
    uint64_t sz();
    void push(uint64_t v);
    uint64_t read(unsigned idx);
    void clear();
    void sort(void);
};

#endif // __SORT_ACCEL_HPP__
