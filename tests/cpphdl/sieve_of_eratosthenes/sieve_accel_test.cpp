#include "sieve_accel.hpp"

#include <boost/random.hpp>
#include <limits>
#include <cassert>

bool test1() {
    SieveAccel sa;
    printf("Beginning sieve test...\n");

    sa.findPrimes();

    printf("   getting primes...\n");
    long p;
    do {
        p = sa.nextPrime();
        printf("    %ld\n", p);
    } while (p >= 0);
    printf("Test exiting\n");
    return true;
}

