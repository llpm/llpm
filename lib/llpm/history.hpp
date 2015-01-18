#ifndef __LLPM_HISTORY_HPP__
#define __LLPM_HISTORY_HPP__

#include <list>
#include <string>

#include <util/macros.hpp>
#include <llpm/exceptions.hpp>

#include <boost/smart_ptr/intrusive_ptr.hpp>

namespace llpm {

// Hey, C++: The 60's want their fwd. decl. back!
class Block;
typedef boost::intrusive_ptr<Block> BlockP;

class BlockHistory {
public:
    enum Source {
        Unset,
        Unknown,
        Frontend,
        Refinement,
        Optimization
    };

private:
    Source _src;
    std::list<BlockP> _srcBlocks;
    std::string _meta;

    BlockHistory(Source s, BlockP sb) :
        _src(s)
    {
        _srcBlocks.push_back(sb);
    }

    BlockHistory(Source s, std::list<BlockP> sb) :
        _src(s),
        _srcBlocks(sb)
    { }

public:
    BlockHistory() :
        _src(Unset)
    { }

    DEF_SET(meta);
    DEF_GET_NP(meta);

    bool hasSrcBlock() const {
        return !_srcBlocks.empty();
    }

    void setUnknown() {
        _src = Unknown;
    }

    void setFrontend(std::string meta="") {
        _src = Frontend;
        _meta = meta;
    }

    void setRefinement(BlockP src) {
        _src = Refinement;
        _srcBlocks = {src};
    }

    void setOptimization(BlockP src) {
        _src = Optimization;
        if (src == NULL)
            _srcBlocks = {};
        else
            _srcBlocks = {src};
    }

    const std::list<BlockP>& srcBlocks() const {
        return _srcBlocks;
    }

    Block* srcBlock() const {
        if (_srcBlocks.size() != 1)
            throw InvalidCall("Can only retrieve single src block when there is only one src block!");
        return _srcBlocks.front().get();
    }

    Source src() const {
        return _src;
    }

    void print(unsigned tabs=0) const;
};

};

#endif // __LLPM_HISTORY_HPP
