#include <stdint.h>

class RegCounter {
    uint64_t _counter;

    RegCounter() :
        _counter(0) { }

public:
    uint64_t read();
    void inc();
    void set(uint64_t v);
};


uint64_t RegCounter::read() {
    return _counter;
}

void RegCounter::inc() {
    _counter += 1;
}

void RegCounter::set(uint64_t v) {
    _counter = v;
}

