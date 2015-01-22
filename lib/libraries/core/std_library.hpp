#ifndef __LLPM_LIBRARIES_CORE_STD_LIBRARY_HPP__
#define __LLPM_LIBRARIES_CORE_STD_LIBRARY_HPP__

/***************
 * This file contains all of the standard functions (and #includes
 * all intrinsics) which backends should consider supporting. If
 * not, libraries can convert these functions to boolean logic.  Any
 * blocks not in this file may be translated to boolean logic and
 * synthesized from boolean functions.
 */

#include <llpm/block.hpp>
#include <libraries/core/comm_intr.hpp>
#include <libraries/core/mem_intr.hpp>
#include <libraries/core/logic_intr.hpp>
#include <refinery/refinery.hpp>

namespace llpm {

    void StdLibStops(BaseLibraryStopCondition& sc);

/*****************
 *   Integer operations
 */

    // Sum of integers
    class IntAddition : public Function {
    public:
        static llvm::Type* InType(std::vector<llvm::Type*>);
        static llvm::Type* OutType(std::vector<llvm::Type*>);

        IntAddition(std::vector<llvm::Type*>);

        virtual bool inputCommutative(InputPort* op) const {
            assert(op == din());
            return true;
        }
    };

    // a - b. Integers only
    class IntSubtraction : public Function {
    public:
        IntSubtraction(llvm::Type* a, llvm::Type* b);
    };

    // Shift by constant amount. Integers only
    class ConstShift : public Function {
    public:
        enum Style {
            LogicalTruncating,
            LogicalExtending,
            Rotating,
            Arithmetic
        };
        static llvm::Type* InType(llvm::Type* a, int shift, Style style);
        static llvm::Type* OutType(llvm::Type* a, int shift, Style style);

    private:
        int _shift;
        Style _style;

    public:
        // a is input type
        // shift is amount -- positive for shift left, 
        ConstShift(llvm::Type* a, int shift, Style style);

        DEF_GET_NP(shift);
        DEF_GET_NP(style);
    };

    // Variable amount shift. Integers only. Truncating only
    class Shift : public Function {
    public:
        enum Direction {
            Left,
            Right
        };
        enum Style {
            Logical,
            Rotating,
            Arithmetic
        };

        static llvm::Type* InType(llvm::Type* a, 
                                  Direction dir, Style style);
        static llvm::Type* OutType(llvm::Type* a,
                                   Direction dir, Style style);

    private:
        Direction _dir;
        Style _style;

    public:
        // a is type of data to be shifted
        // The width of the shift amount operand is log2(a)
        // direction is direction to shift. If 'either', negative
        //   'shift' value is right shift, position is left
        // style defines logical, rotating, or arithmetic shift
        Shift(llvm::Type* a, Direction dir, Style style);

        DEF_GET_NP(dir);
        DEF_GET_NP(style);
    };

    // Truncate by removing some of the most significant bits
    class IntTruncate : public Function {
    public:
        static llvm::Type* InType(unsigned N, llvm::Type*);
        static llvm::Type* OutType(unsigned N, llvm::Type*);
        IntTruncate(unsigned N, llvm::Type* t);
        IntTruncate(llvm::Type* a, llvm::Type* b);
    };

    // Extend an int, adding some MSBs, possibly with sign extension
    class IntExtend: public Function {
        bool _signExtend;
    public:
        static llvm::Type* InType(unsigned N, llvm::Type*);
        static llvm::Type* OutType(unsigned N, llvm::Type*);
        IntExtend(unsigned N, bool signExtend, llvm::Type* t);

        DEF_GET_NP(signExtend);
    };

    // Multiply a bunch of integers
    class IntMultiply : public Function {
    public:
        static llvm::Type* InType(std::vector<llvm::Type*>);
        static llvm::Type* OutType(std::vector<llvm::Type*>);
        IntMultiply(std::vector<llvm::Type*>);

        virtual bool inputCommutative(InputPort* op) const {
            assert(op == din());
            return true;
        }
    };

    // Divide integer a by integer b
    class IntDivide : public Function {
        bool _isSigned;

    public:
        static llvm::Type* InType(llvm::Type* a, llvm::Type* b, bool isSigned);
        static llvm::Type* OutType(llvm::Type* a, llvm::Type* b, bool isSigned);
        IntDivide(llvm::Type* a, llvm::Type* b, bool isSigned);

        DEF_GET_NP(isSigned);
    };

    // Remainder of int a divided by int b
    class IntRemainder : public Function {
        bool _isSigned;

    public:
        static llvm::Type* InType(llvm::Type* a, llvm::Type* b, bool isSigned);
        static llvm::Type* OutType(llvm::Type* a, llvm::Type* b, bool isSigned);
        IntRemainder(llvm::Type* a, llvm::Type* b, bool isSigned);

        DEF_GET_NP(isSigned);
    };

    // Bitwise AND, OR, or XOR of N inputs. All inputs must be of
    // the same type. Integers only
    class Bitwise : public Function {
    public:
        enum Op {
            AND,
            OR,
            XOR
        };

        static llvm::Type* InType(unsigned N, llvm::Type* t);
        static llvm::Type* OutType(unsigned N, llvm::Type* t);

    private:
        Op _op;

    public:
        Bitwise(unsigned N, llvm::Type* t, Op op);
        DEF_GET_NP(op);

        virtual bool inputCommutative(InputPort* op) const {
            assert(op == din());
            return true;
        }
    };

    class IntCompare : public Function {
    public:
        enum Cmp {
            EQ,
            NEQ,
            GT,
            GTE
        };

        static llvm::Type* InType(llvm::Type* a, llvm::Type* b,
                                  Cmp op, bool isSigned);
        static llvm::Type* OutType(llvm::Type* a, llvm::Type* b,
                                   Cmp op, bool isSigned);

    private:
        Cmp _op;
        bool _isSigned;

    public:
        IntCompare(llvm::Type* a, llvm::Type* b,
                   Cmp op, bool isSigned);

        DEF_GET_NP(op);
        DEF_GET_NP(isSigned);

        virtual std::string print() const {
            std::string ret = _isSigned ? "s " : "u ";
            switch (_op) {
            case EQ:
                ret += "==";
                break;
            case NEQ:
                ret += "!=";
                break;
            case GT:
                ret += ">";
                break;
            case GTE:
                ret += ">=";
                break;
            }
            return ret;
        }
    };

} // namespace llpm

#endif // __LLPM_LIBRARIES_CORE_STD_LIBRARY_HPP__
