#ifndef __LLPM_UTIL_MISC_HPP__
#define __LLPM_UTIL_MISC_HPP__

#include <string>

namespace llpm {

std::string cpp_demangle(const char* name);

unsigned inline clog2(uint64_t n) {
    for (unsigned i=64; i>0; i--) {
        if (n & (1ull << (i - 1))) {
            return i - 1;
        }
    }
    return 0;
}

};

#endif //__LLPM_UTIL_MISC_HPP__
