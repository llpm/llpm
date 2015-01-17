#ifndef __SORT_ACCEL_HPP__
#define __SORT_ACCEL_HPP__

#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <limits>
#include <boost/random.hpp>

#define CAPACITY 1024

class LinearAlg {
public:
    uint64_t matrixA[CAPACITY];
    unsigned posA;
    unsigned widthA;
    unsigned heightA;

    uint64_t matrixB[CAPACITY];
    unsigned posB;
    unsigned widthB;
    unsigned heightB;

    uint64_t matrixC[CAPACITY];
    bool validC;
    unsigned posC;
    unsigned widthC;
    unsigned heightC;

public:

    unsigned capacity();

    bool resetA(unsigned w, unsigned h);
    bool pushA(uint64_t);

    bool resetB(unsigned w, unsigned h);
    bool pushB(uint64_t);

    unsigned resetC();
    uint64_t iterC();

    bool multiply();
    bool add();
};

#endif // __SORT_ACCEL_HPP__
