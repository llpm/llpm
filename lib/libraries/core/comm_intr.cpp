#include "comm_intr.hpp"

#include <util/misc.hpp>
#include <util/llvm_type.hpp>
#include <llpm/connection.hpp>

#include <boost/format.hpp>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>

using namespace std;

namespace llpm {
using namespace llvm;

Identity::Identity(llvm::Type* type) :
    Function(type, type)
{
}

Wait::Wait(llvm::Type* type) :
    _din(this, type, "din"),
    _dout(this, type, "dout")
{ }

InputPort* Wait::newControl(llvm::Type* t) {
    auto name = str(boost::format("control%1%") % _controls.size());
    auto ip = new InputPort(this, t, name);
    _controls.emplace_back(ip);
    return ip;
}

void Wait::newControl(ConnectionDB* conns, OutputPort* op) {
    conns->connect(op, newControl(op->type()));
}

Cast::Cast(llvm::CastInst* cast) :
    Function(cast->getSrcTy(), cast->getDestTy()),
    _cast(cast)
{
}

Cast::Cast(llvm::Type* from, llvm::Type* to) :
    Function(from, to),
    _cast(NULL)
{
    if (bitwidth(from) != bitwidth(to))
        throw InvalidArgument("Can only cast to/from types of the same width");
}

Join::Join(const vector<llvm::Type*>& inputs) :
    _dout(this, StructType::get(inputs[0]->getContext(), inputs))
{
    for(auto input: inputs) {
        _din.emplace_back(new InputPort(this, input));
    }
}

Join::Join(llvm::Type* output) :
    _dout(this, output)
{
    llvm::CompositeType* ct = llvm::dyn_cast<llvm::CompositeType>(output);
    if (ct == NULL)
        throw InvalidArgument("When specifying an output type for Join, it must be a CompositeType");
    
    unsigned numTypes = numContainedTypes(ct);
    for (unsigned idx=0; idx<numTypes; idx++) {
        _din.emplace_back(new InputPort(this, ct->getTypeAtIndex(idx)));
    }
}

Join* Join::get(ConnectionDB& conns, const std::vector<OutputPort*>& in) {
    vector<llvm::Type*> types;
    for (auto op: in)
        types.push_back(op->type());
    Join* join = new Join(types);
    for (unsigned i=0; i<in.size(); i++)
        conns.connect(in[i], join->din(i));
    return join;
}

Select::Select(unsigned N, llvm::Type* type) :
    _dout(this, type)
{
    for (unsigned i = 0; i<N; i++) {
        _din.emplace_back(new InputPort(this, type));
    }
}

InputPort* Select::createInput() {
    auto ip = new InputPort(this, _dout.type());
    _din.emplace_back(ip);
    return ip;
}

Split::Split(const vector<llvm::Type*>& outputs) :
    _din(this, StructType::get(outputs[0]->getContext(), outputs))
{
    for(auto output: outputs) {
        _dout.emplace_back(new OutputPort(this, output));
    }
}

Split::Split(llvm::Type* input) :
    _din(this, input)
{
    llvm::CompositeType* ct = llvm::dyn_cast<llvm::CompositeType>(input);
    if (ct == NULL)
        throw InvalidArgument("When specifying an output type for Join, it must be a CompositeType");
    
    unsigned numTypes = numContainedTypes(ct);
    for (unsigned idx=0; idx<numTypes; idx++) {
        _dout.emplace_back(new OutputPort(this, ct->getTypeAtIndex(idx)));
    }
}


bool Split::refineToExtracts(ConnectionDB& conns) const {
    vector<InputPort*> einputs;
    for (unsigned i=0; i<_dout.size(); i++) {
        auto e = new Extract(din()->type(), {i});
        einputs.push_back(e->din());
        conns.remap(_dout[i].get(), e->dout());
    }
    conns.remap(din(), einputs);
    return true;
}

llvm::Type* Extract::GetOutput(llvm::Type* t, std::vector<unsigned> path) {
    for (unsigned i=0; i<path.size(); i++) {
        unsigned idx = path[i];
        if (idx >= numContainedTypes(t))
            throw InvalidArgument("Extract index beyond vector bound!");
        t = nthType(t, idx);
    }
    return t;
}

Extract::Extract(llvm::Type* t, vector<unsigned> path) :
    Function(t, GetOutput(t, path)),
    _path(path)
{ }

llvm::Type* Multiplexer::GetInput(unsigned N, llvm::Type* type) {
    vector<llvm::Type*> types;
    types.push_back(llvm::Type::getIntNTy(type->getContext(), idxwidth(N)));
    for (unsigned i=0; i<N; i++)
        types.push_back(type);
    return llvm::StructType::get(type->getContext(), types);
}

Multiplexer::Multiplexer(unsigned N, llvm::Type* type) :
    Function(GetInput(N, type), type)
{ }

llvm::Type* Router::GetInput(unsigned N, llvm::Type* type) {
    return llvm::StructType::get(
        type->getContext(),
        vector<llvm::Type*>(
            {llvm::Type::getIntNTy(type->getContext(), idxwidth(N)),
             type}));
}

Router::Router(unsigned N, llvm::Type* type) :
    _din(this, GetInput(N, type))
{
    for (unsigned i = 0; i<N; i++) {
        _dout.emplace_back(new OutputPort(this, type));
    }
}

} // namespace llpm
