#ifndef __SORT_ACCEL_HPP__
#define __SORT_ACCEL_HPP__

#include <stdint.h>

// A grid of SIZE by SIZE
#define SIZE 10

class GameOfLife {
public:
    typedef bool State;
    static const bool Alive = true;
    static const bool Dead = false;

    unsigned long steps;
    State state[SIZE*SIZE];
    uint8_t counters[SIZE*SIZE];

public:

    void clear();
    unsigned size();
    void set(unsigned i, unsigned j, State);
    State get(unsigned i, unsigned j);
    void step(unsigned numSteps);
};

#endif // __SORT_ACCEL_HPP__
