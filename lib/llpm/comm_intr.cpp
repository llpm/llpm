#include "comm_intr.hpp"

#include <boost/foreach.hpp>
#include <llvm/IR/DerivedTypes.h>

namespace llpm {
using namespace llvm;

static unsigned clog2(uint64_t n) {
    for (unsigned i=64; i>0; i--) {
        if (n & (1ull << (i - 1))) {
            return i;
        }
    }
    return 0;
}

Identity::Identity(llvm::Type* type) :
    _din(this, type),
    _dout(this, type)
{
}

Cast::Cast(llvm::CastInst* cast) :
    _din(this, cast->getSrcTy()),
    _dout(this, cast->getDestTy()),
    _cast(cast)
{
}

Join::Join(const vector<llvm::Type*>& inputs) :
    _dout(this, StructType::create(inputs))
{
    BOOST_FOREACH(auto input, inputs) {
        _din.push_back(new InputPort(this, input));
    }
}

Select::Select(unsigned N, llvm::Type* type) :
    _dout(this, type)
{
    for (unsigned i = 0; i<N; i++) {
        _din.push_back(new InputPort(this, type));
    }
}

Split::Split(const vector<llvm::Type*>& outputs) :
    _din(this, StructType::create(outputs))
{
    BOOST_FOREACH(auto output, outputs) {
        _dout.push_back(new OutputPort(this, output));
    }
}

Multiplexer::Multiplexer(unsigned N, llvm::Type* type) :
    _sel(this, llvm::Type::getIntNTy(type->getContext(), clog2(N))),
    _dout(this, type)
{
    for (unsigned i = 0; i<N; i++) {
        _din.push_back(new InputPort(this, type));
    }
}

Router::Router(unsigned N, llvm::Type* type) :
    _din(this, type),
    _sel(this, llvm::Type::getIntNTy(type->getContext(), clog2(N)))
{
    for (unsigned i = 0; i<N; i++) {
        _dout.push_back(new OutputPort(this, type));
    }
}

} // namespace llpm
