#include "sha256_accel.hpp"

static boost::random::mt19937 rng;
using namespace std;

template<typename T>
static T myrand() {
    boost::random::uniform_int_distribution<T>
        dist(numeric_limits<T>::min(), numeric_limits<T>::max());
    return dist(rng);
}

bool test1() {
    SHA256 sha;
    sha.start();

    printf("test1 Testing update\n");
    for (unsigned c=0; c<10; c++) {
        SHA256::Data data;
        for (unsigned i=0; i<64; i++)
            data[i] = myrand<uint8_t>();
        sha.update(data);
    }

    printf("test1 Testing digest\n");
    printf("State: ");
    for (unsigned i=0; i<8; i++)
        printf("%08x", sha.state[i]);
    printf("\n");
    sha.digest();

    printf("Apparently everything worked!\n");
    return true;
}

