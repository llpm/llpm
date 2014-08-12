#ifndef __LLPM_STD_LIBRARY_HPP__
#define __LLPM_STD_LIBRARY_HPP__

/***************
 * This file contains all of the standard functions (and #includes
 * all intrinsics) which all backends and libraries should support.
 * Any blocks not in this file may be translated to boolean logic
 * and synthesized from boolean functions.
 */

#include <llpm/block.hpp>
#include <llpm/comm_intr.hpp>
#include <llpm/mem_intr.hpp>
#include <llpm/logic_intr.hpp>

namespace llpm {

/*****************
 *   Integer operations
 */

    // Sum of integers
    class IntAddition : public Function {
    public:
        IntAddition(std::vector<llvm::Type*>);
    };

    // a - b. Integers only
    class IntSubtraction : public Function {
    public:
        IntSubtraction(llvm::Type* a, llvm::Type* b);
    };

    // Shift by constant amount. Integers only
    class ConstShift : public Function {
    public:
        // a is input type
        // shift is amount -- positive for shift left, 
        ConstShift(llvm::Type* a, int shift);
    };

    // Variable amount shift. Integers only
    class Shift : public Function {
    public:
        enum Direction {
            Left,
            Right,
            Either
        };
        enum Style {
            Logical,
            Rotating,
            Arithmetic
        };
        // a is type of data to be shifted
        // shift is amount to be shifted left or right
        // direction is direction to shift. If 'either', negative
        //   'shift' value is right shift, position is left
        // style defines logical, rotating, or arithmetic shift
        Shift(llvm::Type* a, llvm::Type* shift, Direction dir, Style style);
    };


    // Multiply a bunch of integers
    class IntMultiply : public Function {
    public:
        IntMultiply(std::vector<llvm::Type*>);
    };

    // Divide integer a by integer b
    class IntDivide : public Function {
    public:
        IntDivide(llvm::Type* a, llvm::Type* b, bool isSigned);
    };

    // Remainder of int a divided by int b
    class IntRemainder : public Function {
    public:
        IntRemainder(llvm::Type* a, llvm::Type* b, bool isSigned);
    };

    // Bitwise AND, OR, or XOR of N inputs. All inputs must be of
    // the same type. Integers only
    class Bitwise : public Function {
    public:
        Bitwise(unsigned N, llvm::Type* t);
    };

    class IntCompare : public Function {
    public:
        enum Cmp {
            EQ,
            NEQ,
            GT,
            GTE
        };
        IntCompare(llvm::Type* a, llvm::Type* b,
                   Cmp op, bool isSigned);
    };

} // namespace llpm

#endif // __LLPM_STD_LIBRARY_HPP__
