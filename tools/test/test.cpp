#include <cstdio>

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>

using namespace llpm;

int main(void) {
    std::vector<llpm::Block*> vec;
    llpm::Refinery r;
    ConnectionDB conn(NULL);
    r.refine(vec, conn);
    return 0;
}
