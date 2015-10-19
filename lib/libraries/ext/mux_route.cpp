#include "mux_route.hpp"

#include <llpm/connection.hpp>

using namespace std;

namespace llpm {

llvm::Type* SparseMultiplexer::GetInput(llvm::Type* selectorTy,
                                        llvm::Type* dataTy,
                                        const std::set<unsigned>& inputs) {
    std::vector<llvm::Type*> sig;
    sig.push_back(selectorTy);
    sig.push_back(dataTy);
    for (unsigned i=0; i<inputs.size(); i++) {
        sig.push_back(dataTy);
    }

    return llvm::StructType::get(selectorTy->getContext(), sig);
}

SparseMultiplexer::SparseMultiplexer(llvm::Type* selectorTy,
                                     llvm::Type* dataTy,
                                     std::set<unsigned> inputs) :
    Function(GetInput(selectorTy, dataTy, inputs), dataTy),
    _inputs(inputs)
{
}

bool SparseMultiplexer::refine(ConnectionDB& conns) const
{
    //TODO: There are a number of interesting strategies for implementing
    //SparseMultiplexers. The easiest to implement is turning it into a
    //DenseMultiplexer, so that's what I'm implementing now. The TODO part of
    //this is to do something more intelligent, probably depending on exactly
    //how sparse the bloody thing is.
    auto selType = _din.type()->getStructElementType(0);
    auto dataType = _din.type()->getStructElementType(1);
    unsigned selWidth = selType->getIntegerBitWidth();
    unsigned N = 1 << selWidth;
    auto dm = new Multiplexer(N, dataType);

    auto split = new Split(din()->type());
    conns.remap(this->din(), split->din());
    auto join = new Join(dm->din()->type());
    conns.connect(join->dout(), dm->din());
    conns.remap(this->dout(), dm->dout());

    conns.connect(split->dout(0), join->din(0));

    // Connect the non-default inputs
    unsigned idx = 2;
    for (auto input: _inputs) {
        conns.connect(split->dout(idx), join->din(1+input));
        idx++;
    }

    // Connect the default inputs
    for (unsigned i=0; i<N; i++) {
        if (_inputs.count(i) == 0) {
            conns.connect(split->dout(1), join->din(1+i));
        }
    }

    return true;
}

} // namespace llpm
