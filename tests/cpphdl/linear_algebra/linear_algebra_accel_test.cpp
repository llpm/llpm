#include "linear_algebra_accel.hpp"

#include <boost/random.hpp>
#include <limits>
#include <cassert>

using namespace std;

static boost::random::mt19937 rng;

bool test1() {
    boost::random::uniform_int_distribution<uint64_t>
        dist(0, 1000);

    LinearAlg la;
    unsigned capacity = la.capacity();

    boost::random::uniform_int_distribution<uint64_t> d(0, 100);

    printf("Resetting and pushing...\n");
    la.resetA(10, 10);
    la.resetB(10, 10);
    for (unsigned i=0; i<50; i++) {
        la.pushA(d(rng));
        la.pushB(d(rng));
    }

    printf("Adding...\n");
    la.add();
    
    printf("Resetting & iterating...\n");
    unsigned cSize = la.resetC();
    printf("   cSize: %u\n", cSize);
    while (cSize-- > 0) {
        la.iterC();
    }

    printf("Multiplying...\n");
    la.multiply();

    printf("Resetting & iterating...\n");
    cSize = la.resetC();
    while (cSize-- > 0) {
        la.iterC();
    }

    printf("Test exiting\n");
    return true;
}

