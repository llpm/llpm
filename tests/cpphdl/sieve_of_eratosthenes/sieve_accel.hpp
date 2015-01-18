#ifndef __SIEVE_ACCEL_HPP__
#define __SIEVE_ACCEL_HPP__

#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <limits>
#include <boost/random.hpp>

#define CAPACITY 250
#define CAP_SQRT  50

class SieveAccel {
public:
    unsigned pos;
    bool vals[CAPACITY];

    unsigned capacity();
    void findPrimes();
    long nextPrime();
};

#endif // __SIEVE_ACCEL_HPP__
