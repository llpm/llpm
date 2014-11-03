#ifndef __LLPM_HISTORY_HPP__
#define __LLPM_HISTORY_HPP__

#include <list>
#include <string>

#include <llpm/exceptions.hpp>

namespace llpm {

// Hey, C++: The 60's want their fwd. decl. back!
class Block;

class BlockHistory {
public:
    enum Source {
        Unknown,
        Frontend,
        Refinement,
        Optimization
    };

private:
    Source _src;
    std::list<Block*> _srcBlocks;
    std::string _meta;

    BlockHistory(Source s, Block* sb) :
        _src(s)
    {
        _srcBlocks.push_back(sb);
    }

    BlockHistory(Source s, std::list<Block*> sb) :
        _src(s),
        _srcBlocks(sb)
    { }

public:
    BlockHistory() :
        _src(Unknown)
    { }

    void setUnknown() {
        _src = Unknown;
    }

    void setFrontend(std::string meta="") {
        _src = Frontend;
        _meta = meta;
    }

    void setRefinement(Block* src) {
        _src = Refinement;
        _srcBlocks = {src};
    }

    void setOptimization(Block* src) {
        _src = Optimization;
        _srcBlocks = {src};
    }

    const std::list<Block*>& srcBlocks() const {
        return _srcBlocks;
    }

    Block* srcBlock() const {
        if (_srcBlocks.size() != 1)
            throw InvalidCall("Can only retrieve single src block when there is only one src block!");
        return _srcBlocks.front();
    }

    Source src() const {
        return _src;
    }
};

};

#endif // __LLPM_HISTORY_HPP
