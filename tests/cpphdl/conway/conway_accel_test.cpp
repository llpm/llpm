#include "conway_accel.hpp"

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

    GameOfLife gol;
    printf("Beginning GameOfLife test...\n");
    gol.clear();

    printf("   pushing...\n");
    for (unsigned i=0; i<SIZE; i++) {
        for (unsigned j=0; j<SIZE; j++) {
            if (myrand<uint8_t>() > 128)
                gol.set(i, j, GameOfLife::Alive);
        }
    }

    printf("Step 4\n");
    gol.step(4);

    printf("   reading...\n");
    for (unsigned i=0; i<SIZE; i++) {
        for (unsigned j=0; j<SIZE; j++) {
            gol.get(i, j);
        }
    }

    printf("Step 7\n");
    gol.step(7);

    printf("   reading...\n");
    for (unsigned i=0; i<SIZE; i++) {
        for (unsigned j=0; j<SIZE; j++) {
            gol.get(i, j);
        }
    }

    printf("Test exiting\n");
    return true;
}

