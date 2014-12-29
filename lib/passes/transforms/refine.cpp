#include "refine.hpp"

#include <llpm/module.hpp>
#include <set>
#include <vector>

using namespace std;

namespace llpm {

void RefinePass::runInternal(Module* m) {
    // Refine each module down to primitive stopping points
    auto sc = _design.backend()->primitiveStops();
    unsigned passes = m->internalRefine(1, sc);
    assert(passes <= 1);
}

} // namespace llpm
