#ifndef __LLPM_UTIL_MISC_HPP__
#define __LLPM_UTIL_MISC_HPP__

#include <string>

namespace llpm {

std::string cpp_demangle(const char* name);
inline std::string cpp_demangle(const std::string& name) {
    return cpp_demangle(name.c_str());
}

/* How many bits does it take to store an index into N things? */
unsigned inline idxwidth(uint64_t N) {
    if (N == 0)
        return 0;

    // Subtract 1 to make n the largest possible value
    N -= 1;

    // How many shifts to the right does it take to make n zero?
    unsigned shifts = 0;
    while (N != 0) {
        N >>= 1;
        shifts++;
    }
    return shifts;
}

#define DEL_IF(A) \
    if ((A) != NULL) delete (A);

#define DEL_ARRAY(A) \
    { for (auto a: A) { delete a; } }

};

#endif //__LLPM_UTIL_MISC_HPP__
