#include "verilator_extra.h"
#include <cstdio>
#include <map>
#include <algorithm>
#include <string>

using namespace std;

static FILE* debug_output = NULL;
static map<string, uint64_t> counter;

static string toString(unsigned long i) {
    char buf[64];
    snprintf(buf, 64, "0x%lx", i);
    return buf;
}

static string toString(unsigned int i) {
    char buf[64];
    snprintf(buf, 64, "0x%x", i);
    return buf;
}

static string toString(unsigned int*) {
    return "<long>";
}

void start_debug(FILE* f) {
    assert(f != NULL);
    debug_output = f;
}

void close_debug() {
    if (debug_output != NULL) {
        fclose(debug_output);
        debug_output = NULL;
    }
}

template<typename T>
void debug_reg(
    WData* reg_name,
    CData valid1,
    T data1,
    CData valid2,
    T data2
) {
    if (reg_name == NULL)
        return;
    std::string name = (const char*)reg_name;
    std::reverse(name.begin(), name.end());
    counter[name]++;
    if (debug_output != NULL) {
        if (valid1 && valid2) {
            fprintf(debug_output, "[%6lu] %30s, %25s, %25s\n",
                    counter[name], name.c_str(),
                    toString(data1).c_str(),
                    toString(data2).c_str());
        } else if (valid1) {
            fprintf(debug_output, "[%6lu] %30s, %25s\n",
                    counter[name], name.c_str(),
                    toString(data1).c_str());
        } else if (valid2) {
            fprintf(debug_output, "[%6lu] %30s, %25s, %25s\n",
                    counter[name], name.c_str(),
                    "", 
                    toString(data2).c_str());
        } else {
            fprintf(debug_output, "[%6lu] %30s\n",
                    counter[name], name.c_str());
        }
        fflush(debug_output);
    }
}

template<typename T>
void debug_reg(
    WData* reg_name,
    CData valid1,
    T data1
) {
    debug_reg(reg_name, valid1, data1, 0, (T)0);
}


#define INSTANTIATE_DEBUG_REG(T) \
    template void debug_reg<T>( \
        WData* reg_name, \
        CData valid1, \
        T data1, \
        CData valid2, \
        T data2 ) ; \
    template void debug_reg<T>( \
        WData* reg_name, \
        CData valid1, \
        T data1 ) ;

INSTANTIATE_DEBUG_REG(unsigned long);
INSTANTIATE_DEBUG_REG(unsigned int);
INSTANTIATE_DEBUG_REG(unsigned int*);
