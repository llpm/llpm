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

#if 0
void RegCounter::inc() {
    _counter += 1;
}
#endif

void RegCounter::set(uint64_t v) {
    _counter = v;
}

