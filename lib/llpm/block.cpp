#include "block.hpp"

namespace llpm {


bool Block::refine(std::vector<Block*>& blocks) {
    if (!this->refinable())
        return false;
    assert(false && "Not yet implemented!");
}

} // namespace llpm
