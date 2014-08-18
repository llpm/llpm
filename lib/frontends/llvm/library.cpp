#include "library.hpp"

namespace llpm {

template<typename T>
class LLVMRefiner : public BlockRefiner {
public:
    virtual bool handles(Block* b) const {
        return dynamic_cast<LLVMBasicBlock*>(b) != NULL;
    }
};

class LLVMBasicBlockRefiner : public LLVMRefiner<LLVMBasicBlock> {
public:
    virtual bool refine(
            const Block* block,
            std::vector<Block*>& blocks,
            std::map<InputPort*, vector<InputPort*> >& ipMap,
            std::map<OutputPort*, OutputPort*>& opMap) const
    {
        return false;
    }
};

std::vector<Refinery<Block>::Refiner*> LLVMBaseLibrary::BuildCollection() {
    return {
        new LLVMBasicBlockRefiner(),
    };
}

} // namespace llpm

