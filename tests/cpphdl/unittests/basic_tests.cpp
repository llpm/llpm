#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <limits>

#include <boost/random.hpp>

using namespace std;

class BasicTests {
    uint64_t dummyData;
    bool cond;
    unsigned tests;
    uint64_t mem[8];

public:
    uint64_t addUU(uint64_t a, uint64_t b);
    int64_t addUS(uint64_t a, int64_t b);
    int64_t addSS(int64_t a, int64_t b);

    void set(unsigned idx, uint64_t v);
    uint64_t memadd(unsigned i, unsigned j);

    void setCond(bool c);
    bool condTest(unsigned i);
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

void BasicTests::set(unsigned idx, uint64_t v) {
    mem[idx] = v;
}

uint64_t BasicTests::memadd(unsigned i, unsigned j) {
    return mem[i] + mem[j];
}

void BasicTests::setCond(bool c) {
    cond = c;
    tests = 0;
}

bool BasicTests::condTest(unsigned i) {
    if (cond || i > 64)
        return false;
    tests += 1;
    return true;
}

static boost::random::mt19937 rng;

template<typename T>
static T myrand() {
    boost::random::uniform_int_distribution<T>
        dist(numeric_limits<T>::min(), numeric_limits<T>::max());
    return dist(rng);
}

bool test1() {
    BasicTests tests;

    tests.set(1, 65);
    tests.set(3, 98);
    printf("memadd... ");
    fflush(stdout);
    tests.memadd(1, 3);
    printf("done\n");
    tests.addSS(numeric_limits<int64_t>::min(),
                numeric_limits<int64_t>::min());
    tests.addSS(numeric_limits<int64_t>::min(),
                numeric_limits<int64_t>::max());
    tests.addSS(numeric_limits<int64_t>::max(),
                numeric_limits<int64_t>::min());
    for (unsigned i=0; i<1000; i++) {
        tests.addUU(myrand<uint64_t>(), myrand<uint64_t>());
        tests.addUS(myrand<uint64_t>(), myrand<int64_t>());
        tests.addSS(myrand<int64_t>(), myrand<int64_t>());
    }

    tests.setCond(false);
    tests.condTest(64);

    printf("All tests passed!\n");
    return true;
}

