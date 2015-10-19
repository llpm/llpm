#ifndef __LLPM_LIBRARIES_EXT_MUX_ROUTE_HPP__
#define __LLPM_LIBRARIES_EXT_MUX_ROUTE_HPP__

#include <llpm/block.hpp>
#include <libraries/core/comm_intr.hpp>
#include <set>

namespace llpm {

/**
 * A multiplexer with sparse inputs and a default input. For a selector type
 * of i3, for instance, up to 8 inputs may be (all optionally) specified. If
 * the selector value is one of those 8, that value is selected. If not, the
 * default value is selected. Consumes all inputs and destroys unselected ones.
 */
class SparseMultiplexer : public Function {
    static llvm::Type* GetInput(llvm::Type* selectorTy,
                                llvm::Type* dataTy,
                                const std::set<unsigned>& inputs);

    std::set<unsigned> _inputs;
public:
    SparseMultiplexer(llvm::Type* selectorTy,
                      llvm::Type* dataTy,
                      std::set<unsigned> inputs);
    virtual ~SparseMultiplexer() { }

    InputPort* getInput(ConnectionDB& conns, unsigned idx) {
        unsigned inputIdx = 0;
        for (unsigned i: _inputs) {
            if (idx == i)
                break;
            inputIdx++;
        }
        assert(inputIdx < _inputs.size() && "Idx must be in set!");
        return din()->join(conns)->din(2 + inputIdx);
    }
    InputPort* getDefault(ConnectionDB& conns) {
        return din()->join(conns)->din(1);
    }
    InputPort* getSelector(ConnectionDB& conns) {
        return din()->join(conns)->din(0);
    }

    bool refine(ConnectionDB& conns) const;
};

}

#endif
