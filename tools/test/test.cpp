#include <cstdio>

#include <llpm/module.hpp>
#include <llpm/std_library.hpp>
#include <refinery/refinery.hpp>

int main(void) {
    llpm::Refinery<llpm::Block> r;
    r.refine({});
    return 0;
}
