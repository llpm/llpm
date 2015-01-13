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

    // Scratchpad for Merge function
    uint64_t merge[CAPACITY];

private:
    __attribute__((always_inline))
    void Merge(unsigned start, unsigned middle, unsigned end) {
        unsigned l = 0, r = 0, i = 0;
        while (l < middle - start && r < end - middle)
        {
            merge[i++] = (vals[start + l] < vals[middle + r])
                ? vals[start + l++]
                : vals[middle + r++];
        }
     
        while (r < end - middle) merge[i++] = vals[middle + r++];
        while (l < middle - start) merge[i++] = vals[start + l++];

        for (unsigned i=0; i<(end-start); i++) {
            vals[i + start] = merge[i];
        }
    }

public:
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
