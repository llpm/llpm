#include "sieve_accel.hpp"

unsigned SieveAccel::capacity() {
    return CAPACITY;
}

void SieveAccel::findPrimes() {
    for (unsigned i=0; i<CAPACITY; i++)
        vals[i] = true;

    for (unsigned i=2; i<CAP_SQRT; i++) {
        if (vals[i]) {
            for (unsigned j=i*i; j<CAPACITY; j+=i) {
                vals[j] = false;
            }
        }
    }

    pos = 0;
}

long SieveAccel::nextPrime() {
    while (pos < CAPACITY && vals[pos] == false)
        pos++;
    if (pos == CAPACITY)
        return -1;
    return pos;
}

