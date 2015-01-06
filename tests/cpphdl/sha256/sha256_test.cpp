#include "SHA256.hpp"

#include <cstdio>
#include <stdlib.h>

#define SIZE 10

int main() {
    char c;
    uint64_t l;

    SHA256* sha = new SHA256();
    sha->trace("debug.vcd");
    sha->reset();
    uint64_t start = sha->cycles();

    printf("Initializing...\n");
    sha->start_req();
    sha->start_resp(&c);

    printf("Sending data block...\n");
    for (unsigned c=0; c<10; c++) {
        uint8_t data[64];
        for (unsigned i=0; i<64; i++) {
            data[i] = rand();
        }
        sha->update_req(data);
        sha->update_resp(&c);
    }

    printf("Getting digest...\n");
    uint8_t digest[32];
    sha->digest_req();
    sha->digest_resp(digest);

    for (unsigned i=0; i<32; i++) {
        printf("%02x", digest[i]);
    }
    printf("\n");

    sha->run(2);
    delete sha;
    return 0;
}
