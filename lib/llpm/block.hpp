#ifndef __LLPM_BLOCK_HPP__
#define __LLPM_BLOCK_HPP__

#include <vector>
#include <llpm/ports.hpp>
#include <llpm/exceptions.hpp>

using namespace std;

namespace llpm {

#define DEF_GET(F) auto F() const -> const decltype(_##F)* { return &_##F; }
#define DEF_ARRAY_GET(F) \
    unsigned F##_size() { return _##F.size(); } \
    auto F(unsigned i) const -> const decltype(_##F)::value_type* { return &_##F[i]; }

class Block {
protected:
    vector<InputPort*>  _inputs;
    vector<OutputPort*> _outputs;

    Block() { }
    virtual ~Block() { }

public:
    vector<InputPort*>&  inputs()  {
        return _inputs;
    }
    vector<OutputPort*>& outputs() {
        return _outputs;
    }

    virtual bool hasState() const = 0;
    bool isPure() const {
        return !hasState();
    }
};

/*****
 * A purely functional block. 
 * Takes one input and produces one output. No state
 *
 * Can sometimes be refined into smaller, more granular functions
 * via "refine" method.
 */
class Function : public Block {
protected:
    InputPort _din;
    OutputPort _dout;

    Function(llvm::Type* input, llvm::Type* output) :
        _din(this, input),
        _dout(this, output) {

        _inputs = {&_din};
        _outputs = {&_dout};
    }

    virtual ~Function() { }

public:
    virtual bool hasState() const {
        return false;
    }

    DEF_GET(din);
    DEF_GET(dout);
    
    virtual bool refinable() const {
        return false;
    }
    virtual bool refine(std::vector<Function*>&) {
        if (refinable())
            throw ImplementationError("Block needs to implement refine method!");
        return false;
    }
};

} // namespace llpm

#endif // __LLPM_BLOCK_HPP__
