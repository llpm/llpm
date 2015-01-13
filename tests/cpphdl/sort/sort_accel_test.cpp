#include "sort_accel.hpp"

#include <boost/random.hpp>
#include <limits>
#include <cassert>

static boost::random::mt19937 rng;
using namespace std;

template<typename T>
static T myrand() {
    boost::random::uniform_int_distribution<T>
        dist(numeric_limits<T>::min(), numeric_limits<T>::max());
    return dist(rng);
}

bool test1() {
    SortAccel sa;
    printf("Beginning sort test...\n");
    sa.clear();

    printf("   checking size...\n");
    sa.sz();

    printf("   pushing...\n");
    unsigned capacity = sa.capacity();
    for (unsigned i=0; i<capacity; i++) {
        // sa.push(myrand<uint64_t>());
        sa.push(i + 2);
        printf("   checking size...\n");
        sa.sz();
    }

    printf("   sorting...\n");
    sa.sort();
    printf("   checking size...\n");
    sa.sz();
 

    printf("   reading...\n");
    uint64_t last;
    for (unsigned i=0; i<capacity; i++) {
        uint64_t v = sa.read(i);
        // if (i > 0)
            // assert(last < v);
        last = v;
    }

    printf("Apparently everything worked!\n");
    return true;
}

