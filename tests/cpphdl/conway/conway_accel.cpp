#include "conway_accel.hpp"

void GameOfLife::clear() {
    steps = 0;
    for (unsigned i=0; i<(SIZE*SIZE); i++) {
        state[i] = Dead;
    }
}

unsigned GameOfLife::size() {
    return SIZE;
}

void GameOfLife::set(unsigned i, unsigned j, State s) {
    state[i + j*SIZE] = s;
}

GameOfLife::State GameOfLife::get(unsigned i, unsigned j) {
    return state[i + j*SIZE];
}

__attribute__((always_inline))
void step(GameOfLife::State* board, uint8_t* counters) {
    for (unsigned i=0; i<SIZE*SIZE; i++) {
        counters[i] = 0;
    }

    for (unsigned i=0; i<SIZE; i++) {
        for (unsigned j=0; j<SIZE; j++) {
            auto s = board[i + j*SIZE];
            if (s) {
                if (i > 0) counters[(i-1) + (j)*SIZE] += 1;
                if (i+1 < SIZE) counters[(i+1) + (j)*SIZE] += 1;
                if (j+1 < SIZE) counters[(i) + (j+1)*SIZE] += 1;
                if (j > 0) counters[(i) + (j-1)*SIZE] += 1;
                if (i > 0 && j > 0) counters[(i-1) + (j-1)*SIZE] += 1;
                if (i+1 < SIZE && j+1 < SIZE) counters[(i+1) + (j+1)*SIZE] += 1;
                if (i > 0 && j+1 < SIZE) counters[(i-1) + (j+1)*SIZE] += 1;
                if (i+1 < SIZE && j > 0) counters[(i+1) + (j-1)*SIZE] += 1;
            }
        }
    }

    for (unsigned i=0; i<SIZE*SIZE; i++) {
        auto count = counters[i];
        auto live = board[i];
        GameOfLife::State newState = live;
        if (live == GameOfLife::Alive) {
            if (count < 2) {
                newState = GameOfLife::Dead;
            } else if (count > 3) {
                newState = GameOfLife::Dead;
            }
        } else {
            if (count == 3)
                newState = GameOfLife::Alive;
        }
        board[i] = newState;
    }
}

void GameOfLife::step(unsigned numSteps) {
    for (unsigned s=0; s<numSteps; s++) {
        ::step(state, counters);
        steps++;
    }
}
