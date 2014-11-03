#include "logic_intr.hpp"

#include <boost/foreach.hpp>

namespace llpm {
using namespace llvm;

BooleanLogic::BooleanLogic(unsigned F,
                           llvm::Type* inputType,
                           llvm::Type* outputType) :
    Function(inputType, outputType)
{
}

} // namespace llpm
