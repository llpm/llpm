#include "SHA256.hpp"

#include <cstdio>

#define SIZE 10

int main() {
    char c;
    uint64_t l;

    SHA256* sha = new SHA256();
    sha->trace("debug.vcd");
    sha->reset();
    uint64_t start = sha->cycles();

    sha->run(2);
    delete sha;
    return 0;
}
