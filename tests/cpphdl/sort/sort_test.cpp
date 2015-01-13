#include "SortAccel.hpp"

#include <cstdio>
#include <cassert>
#include <stdlib.h>
#include <string.h>

#include <boost/random.hpp>
#include <limits>

using namespace std;

static boost::random::mt19937 rng;

template<typename T>
static T myrand() {
    boost::random::uniform_int_distribution<T>
        dist(numeric_limits<T>::min(), numeric_limits<T>::max());
    return dist(rng);
}

int main() {
    SortAccel* sa = new SortAccel();
    sa->trace("debug.vcd");
    sa->reset();

#if 0
    if (!sa->test1()) {
        printf("TEST FAIL!\n");
    } else {
        printf("TEST PASS!\n");
    }
#endif

    sa->clear();
    unsigned capacity = sa->capacity();
    for (unsigned i=0; i<capacity; i++) {
        sa->push(myrand<uint64_t>());
    }

    uint64_t start = sa->cycles();
    sa->sort();
    uint64_t stop = sa->cycles();
    printf("Sorting cycles: %lu\n", stop - start);

    unsigned sz = sa->sz();
    uint64_t last;
    bool badSort = false;
    for (unsigned i=0; i<sz; i++) {
        uint64_t v = sa->read(i);
        if (i > 0) {
            if (last > v)
                badSort = true;
        }
    }

    if (badSort) {
        printf("ERROR: Values improperly sorted!\n");
    } else {
        printf("Values returned in sorted order\n");
    }

    sa->run(5);
    delete sa;
    return 0;
}

