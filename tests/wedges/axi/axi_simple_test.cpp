#include "AxiSimple_axi.hpp"

#include <cstdio>

int main() {
    AxiSimple_axi* axi = new AxiSimple_axi();
    //axi->trace("debug.vcd");
    axi->reset();

    uint8_t rc;
    uint64_t data;

    /*** Do set **/
    printf("=== Running 'set'\n");
    // Write args for set
    printf("Writing address\n");
    axi->writeAddress(16, 0);
    printf("Writing data\n");
    axi->writeData(5, 0);

    printf("Waiting for write resp...\n");
    axi->writeResp(&rc);
    printf("    rc: %u\n", (unsigned)rc);

    // Read response from set
    printf("Sending read address\n");
    axi->readAddress(40, 0);
    printf("Reading read data...\n");
    axi->readData(&data, &rc);
    printf("    data: %lu rc: %u\n", data, (unsigned)rc);


    /*** Do read **/
    printf("=== Running 'read'\n");
    // Write args for read
    printf("Writing address\n");
    axi->writeAddress(0, 0);
    printf("Writing data\n");
    axi->writeData(0, 0);

    printf("Waiting for write resp...\n");
    axi->writeResp(&rc);
    printf("    rc: %u\n", (unsigned)rc);

    // Read response from set
    printf("Sending read address\n");
    axi->readAddress(24, 0);
    printf("Reading read data...\n");
    axi->readData(&data, &rc);
    printf("    data: %lu rc: %u\n", data, (unsigned)rc);


    axi->run(2);
    delete axi;
    return 0;
}
