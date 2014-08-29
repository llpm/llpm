#include <cstdint>

class TestClass {
    uint64_t i;
    uint64_t array[128];

public:
    void inc();
    void reset();
    uint64_t read();

    uint64_t readLoc(unsigned idx);
};

void TestClass::inc() {
    i = i + 1;
}

void TestClass::reset() {
    i = 0;
}

uint64_t TestClass::read() {
    return i;
}

uint64_t TestClass::readLoc(unsigned idx) {
    return array[idx];
}

int main() {
    return 0;
}
