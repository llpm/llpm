#include "comm_intr.hpp"

#include <boost/foreach.hpp>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>

namespace llpm {
using namespace llvm;

static unsigned clog2(uint64_t n) {
    for (unsigned i=64; i>0; i--) {
        if (n & (1ull << (i - 1))) {
            return i - 1;
        }
    }
    return 0;
}

Identity::Identity(llvm::Type* type) :
    Function(type, type)
{
}

Cast::Cast(llvm::CastInst* cast) :
    Function(cast->getSrcTy(), cast->getDestTy()),
    _cast(cast)
{
}

Join::Join(const vector<llvm::Type*>& inputs) :
    _dout(this, StructType::get(inputs[0]->getContext(), inputs))
{
    for(auto input: inputs) {
        _din.push_back(new InputPort(this, input));
    }
}

Join::Join(llvm::Type* output) :
    _dout(this, output)
{
    llvm::CompositeType* ct = llvm::dyn_cast<llvm::CompositeType>(output);
    if (ct == NULL)
        throw InvalidArgument("When specifying an output type for Join, it must be a CompositeType");
    
    unsigned idx = 0;
    while (ct->indexValid(idx)) {
        _din.push_back(new InputPort(this, ct->getTypeAtIndex(idx)));
        idx += 1;
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
    _din(this, StructType::get(outputs[0]->getContext(), outputs))
{
    for(auto output: outputs) {
        _dout.push_back(new OutputPort(this, output));
    }
}

Extract::Extract(llvm::Type* t, vector<unsigned> path) :
    Function(t,llvm::ExtractValueInst::getIndexedType(t, path))
{ }

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
