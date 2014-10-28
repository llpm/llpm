#include <stdio.h>

int add(int a, int b) {
    if (a < 0)
        a *= -1;
    if (b < 0) {
        b *= a;
    }
    return a + b;
}

struct TestStruct {
    unsigned int a;
    unsigned long b;
};

struct TestStruct ts(struct TestStruct r) {
    r.a += 1;
    return r;
}

long foo(long a, unsigned n) {
    unsigned i;
    for (i=0; i<n; i++) {
        a *= i;
    }
    return a;
}

long array(int n) {
    int arr[128];

    for (unsigned i=0; i<128; i++) {
        arr[i] = n * i;
    }

    long total = 0;
    for (unsigned i=0; i<128; i++) {
        total += arr[i];
    }
    return total / 128;
}

int main(int argc, const char** argv) {
    return 0;
}
