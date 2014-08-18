#include <cstdio>

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <llpm/std_library.hpp>
#include <refinery/refinery.hpp>

int main(void) {
    std::vector<llpm::Block*> vec;
    llpm::Refinery<llpm::Block> r;
    r.refine(vec);
    return 0;
}
