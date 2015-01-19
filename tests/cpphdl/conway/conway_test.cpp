#include "GameOfLife.hpp"

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
    GameOfLife* gol = new GameOfLife();
    //gol->trace("debug.vcd");
    gol->reset();
    unsigned size = gol->size();

    if (!gol->test1()) {
        printf("TEST FAIL!\n");
    } else {
        printf("TEST PASS!\n");
    }

    printf("Running the game of life!\n");
    gol->clear();

    printf("   pushing...\n");
    for (unsigned i=0; i<size; i++) {
        for (unsigned j=0; j<size; j++) {
            if (myrand<uint8_t>() > 200) {
                gol->set(i, j, 1);
                putchar('-');
            } else {
                putchar(' ');
            }
        }
        puts("");
    }
    puts("");


    for (unsigned step=0; step<5; step++) {
        unsigned c = gol->cycles();
        gol->step(1);
        printf("Cycles: %lu\n", (gol->cycles() - c));

        for (unsigned i=0; i<size; i++) {
            for (unsigned j=0; j<size; j++) {
                auto s = gol->get(i, j);
                if (s)
                    putchar('-');
                else
                    putchar(' ');
            }
            puts("");
        }
        puts("");
    }


    unsigned c = gol->cycles();
    gol->step(100);
    printf("Avg cycles/step: %f\n", (gol->cycles() - c) / 100.0);
    for (unsigned i=0; i<size; i++) {
        for (unsigned j=0; j<size; j++) {
            auto s = gol->get(i, j);
            if (s)
                putchar('-');
            else
                putchar(' ');
        }
        puts("");
    }
    puts("");

    gol->run(5);
    delete gol;
    return 0;
}

