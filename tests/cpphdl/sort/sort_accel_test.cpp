#include "sort_accel.hpp"

#include <boost/random.hpp>
#include <limits>
#include <cassert>

using namespace std;

static boost::random::mt19937 rng;

template<typename T>
static T myrand() {
    boost::random::uniform_int_distribution<T>
        dist(numeric_limits<T>::min(), numeric_limits<T>::max());
    return dist(rng);
}

bool test1() {
    boost::random::uniform_int_distribution<uint64_t>
        dist(0, 1000);

    SortAccel sa;
    printf("Beginning sort test...\n");
    sa.clear();

    printf("   checking size...\n");
    sa.sz();

    printf("   pushing...\n");
    unsigned capacity = sa.capacity();
    for (unsigned i=0; i<capacity; i++) {
        // sa.push(myrand<uint64_t>());
        // sa.push(400 - i);
        sa.push(dist(rng));
        sa.sz();
    }

    printf("   sorting...\n");
    sa.sort();
    printf("   checking size...\n");
    sa.sz();
 
#if 0
    for (unsigned i=0; i<capacity; i++) {
        printf("%lu\n", sa.vals[i]);
    }
#endif

    printf("   reading...\n");
    for (unsigned i=0; i<capacity; i++) {
        uint64_t v = sa.read(i);
    }

    printf("Test exiting\n");
    return true;
}

