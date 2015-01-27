#include <stdint.h>

class AxiSimple {
    uint64_t _counter;

    AxiSimple() :
        _counter(0) { }

public:
    uint64_t read();
    void inc();
    void set(uint64_t v);
};


uint64_t AxiSimple::read() {
    return _counter;
}

void AxiSimple::inc() {
    _counter += 1;
}

void AxiSimple::set(uint64_t v) {
    _counter = v;
}

