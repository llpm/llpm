#include "LinearAlg.hpp"

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
    LinearAlg* la = new LinearAlg();
    // la->trace("debug.vcd");
    la->reset();

#if 1
    if (!la->test1()) {
        printf("TEST FAIL!\n");
    } else {
        printf("TEST PASS!\n");
    }
#endif

    boost::random::uniform_int_distribution<uint64_t> d(0, 10000);

    printf("Resetting and pushing...\n");
    la->resetA(10, 25);
    la->resetB(30, 10);
    for (unsigned i=0; i<300; i++) {
        la->pushA(d(rng));
    }

    for (unsigned i=0; i<400; i++) {
        la->pushB(d(rng));
    }

    unsigned start = la->cycles();
    bool rc = la->multiply();
    if (!rc)
        printf("Refused to multiply!\n");
    printf("Cycles to multiply: %lu\n", la->cycles() - start);

    printf("Resetting and pushing...\n");
    la->resetA(30, 30);
    la->resetB(30, 30);
    for (unsigned i=0; i<900; i++) {
        la->pushA(d(rng));
        la->pushB(d(rng));
    }

    start = la->cycles();
    rc = la->add();
    if (!rc)
        printf("Refused to add!\n");
    printf("Cycles to add: %lu\n", la->cycles() - start);

    la->run(5);
    delete la;
    return 0;
}

