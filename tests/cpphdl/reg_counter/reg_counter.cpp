#include <stdint.h>

class RegCounter {
    uint64_t _counter;

    RegCounter();

public:
    void inc();
    void set(uint64_t v);
};


RegCounter::RegCounter() :
    _counter(0)
{ }

void RegCounter::inc() {
    _counter += 1;
}

void RegCounter::set(uint64_t v) {
    _counter = v;
}

