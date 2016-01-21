#ifndef __VERILATOR_EXTRA_H__
#define __VERILATOR_EXTRA_H__

#include "verilated.h"

extern uint64_t* globalCycleCounter;

template<typename T>
void debug_reg(
    WData* reg_name,
    CData valid1,
    T data1,
    CData valid2,
    T data2
);

template<typename T>
void debug_reg(
    unsigned long long reg_name,
    CData valid1,
    T data1,
    CData valid2,
    T data2
) {
    char name[9];
    *(unsigned long long*)name = reg_name;
    name[8] = 0;
    debug_reg((WData*)name, valid1, data1, valid2, data2);
}

template<typename T>
void debug_reg(
    unsigned int reg_name,
    CData valid1,
    T data1,
    CData valid2,
    T data2
) {
    char name[5];
    *(unsigned int*)name = reg_name;
    name[4] = 0;
    debug_reg((WData*)name, valid1, data1, valid2, data2);
}

template<typename T>
void debug_reg(
    WData* reg_name,
    CData valid1,
    T data1
);

template<typename T>
static void debug_reg(
    unsigned long long reg_name,
    CData valid1,
    T data1
) {
    char name[9];
    *(unsigned long long*)name = reg_name;
    name[8] = 0;
    debug_reg((WData*)name, valid1, data1);
}

template<typename T>
static void debug_reg(
    unsigned int reg_name,
    CData valid1,
    T data1
) {
    char name[5];
    *(unsigned int*)name = reg_name;
    name[4] = 0;
    debug_reg((WData*)name, valid1, data1);
}


void start_debug(FILE* f);
void close_debug();

#endif //__VERILATOR_EXTRA_H__
