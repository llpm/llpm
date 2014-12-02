#include "logic_intr.hpp"

#include <util/llvm_type.hpp>

#include <boost/foreach.hpp>

namespace llpm {
using namespace llvm;

BooleanLogic::BooleanLogic(unsigned F,
                           llvm::Type* inputType,
                           llvm::Type* outputType) :
    Function(inputType, outputType)
{
}

std::string Constant::print() const {
    if (this->_value == NULL)
        return "(null)";
    return valuestr(this->_value);
}

} // namespace llpm
