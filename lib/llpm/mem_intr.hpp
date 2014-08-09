#ifndef __LLPM_MEM_INTR_HPP__
#define __LLPM_MEM_INTR_HPP__

#include <llpm/block.hpp>
#include <llvm/IR/InstrTypes.h>

namespace llpm {

// Accepts a single input and stores it. Outputs the stored
// input when a request arrives. Also supports a reset signal,
// which invalidates the contents. Requests on invalid data are held
// until input data arrives.
class Register : public Block {
    InputPort _din;
    InputPort _reset;
    InputPort _req;
    OutputPort _dout;

public:
    Register(llvm::Type* type);
    virtual ~Register() { }

    virtual bool hasState() const {
        return true;
    }

    DEF_GET(din);
    DEF_GET(reset);
    DEF_GET(req);
    DEF_GET(dout);
};

} // namespace llpm

#endif // __LLPM_MEM_INTR_HPP__
