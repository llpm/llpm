#include "BasicTests.hpp"
#include <cstdio>

int main() {
    BasicTests* bt = new BasicTests();
    //bt->trace("debug.vcd");
    bt->reset();

    bool passed = bt->test1();
    if (!passed)
        printf("TEST FAIL\n");

    bt->run(2);
    delete bt;
    return 0;
}

