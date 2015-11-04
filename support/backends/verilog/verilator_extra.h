#ifndef __VERILATOR_EXTRA_H__
#define __VERILATOR_EXTRA_H__

#include "verilated.h"

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
    WData* reg_name,
    CData valid1,
    T data1
);

void start_debug(FILE* f);
void close_debug();

#endif //__VERILATOR_EXTRA_H__
