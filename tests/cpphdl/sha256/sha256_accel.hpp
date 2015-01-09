#ifndef __SHA256_ACCEL_HPP__
#define __SHA256_ACCEL_HPP__

#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <limits>
#include <boost/random.hpp>

class SHA256 {
public:
    typedef uint8_t Digest __attribute__((__vector_size__(32)));
    typedef uint8_t Data __attribute__((__vector_size__(64)));
    typedef uint32_t State __attribute__((__vector_size__(32)));

private:
    // uint32_t total[2];
    // uint32_t state[8];
    State state;

public:
    SHA256() {
    }

    void start();
    void update(Data input);
    Digest digest();
};

#endif // __SHA256_ACCEL_HPP__
