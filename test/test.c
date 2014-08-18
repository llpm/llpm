#include <stdio.h>

int add(int a, int b) {
    if (a < 0)
        a *= -1;
    if (b < 0) {
        b *= a;
    }
    return a + b;
}

int main(int argc, const char** argv) {
    return 0;
}
