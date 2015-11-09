#ifndef __LLPM_ANALYSIS_CONSTANT_HPP__
#define __LLPM_ANALYSIS_CONSTANT_HPP__

#include <llpm/ports.hpp>
#include <llvm/IR/Constant.h>

namespace llpm {

// If the argument port is constant, try to resolve the value to a
// compile-time constant
llvm::Constant* EvalConstant(const ConnectionDB* conns, OutputPort*);

} // namespace llpm

#endif // __LLPM_ANALYSIS_CONSTANT_HPP__
