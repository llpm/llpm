#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <limits>

#include <boost/random.hpp>

using namespace std;

class BasicTests {
    uint64_t dummyData;

public:
    uint64_t addUU(uint64_t a, uint64_t b);
    int64_t addUS(uint64_t a, int64_t b);
    int64_t addSS(int64_t a, int64_t b);
};

uint64_t BasicTests::addUU(uint64_t a, uint64_t b) {
    return a+b;
}
int64_t BasicTests::addUS(uint64_t a, int64_t b) {
    return a+b;
}
int64_t BasicTests::addSS(int64_t a, int64_t b) {
    return a+b;
}

static boost::random::mt19937 rng;

template<typename T>
static T myrand() {
    boost::random::uniform_int_distribution<T>
        dist(numeric_limits<T>::min(), numeric_limits<T>::max());
    return dist(rng);
}

void test1() {
    BasicTests tests;
    for (unsigned i=0; i<1000; i++) {
        tests.addUU(myrand<uint64_t>(), myrand<uint64_t>());
    }

    printf("All tests passed!\n");
}

