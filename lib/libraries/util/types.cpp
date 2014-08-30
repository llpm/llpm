#include "types.hpp"

#include <llpm/comm_intr.hpp>

#include <llvm/IR/DerivedTypes.h>

namespace llpm {

llvm::Type* StructTwiddler::GetOutput(llvm::Type* input,
                                      const std::vector<unsigned>& mapping)
{
    if (mapping.size() == 0)
        throw InvalidArgument("StructTwiddler needs a non-null mapping!");

    std::vector<llvm::Type*> outVec;
    for(unsigned i: mapping) {
        if (i >= input->getStructNumElements())
            throw InvalidArgument("Index in mapping to StructTwiddler exceeds size of input struct");
        outVec.push_back(input->getStructElementType(i));
    }
    return llvm::StructType::get(input->getContext(), outVec);
}

StructTwiddler::StructTwiddler(llvm::Type* input,
                               const std::vector<unsigned>& mapping) :
    Function(input, GetOutput(input, mapping)),
    _mapping(mapping) 
{ }

bool StructTwiddler::refine(ConnectionDB& conns) const
{
    Split* split = new Split(this->din()->type());
    Join*  join  = new Join(this->dout()->type());

    unsigned ji = 0;
    for (unsigned si: _mapping) {
        conns.connect(split->dout(si), join->din(ji));
        ji += 1;
    }

    conns.remap(this->din(), {split->din()});
    conns.remap(this->dout(), join->dout());

    return true;
}

} // namespace llpm
