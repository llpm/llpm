#ifndef __LLPM_VERILOG_SYNTHESIZE_HPP__
#define __LLPM_VERILOG_SYNTHESIZE_HPP__

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <frontends/llvm/objects.hpp>
#include <synthesis/object_namer.hpp>
#include <refinery/priority_collection.hpp>

// fwd def
namespace llvm {
    class Module;
}

namespace llpm {

class VerilogSynthesizer {
public:
    class Context {
        std::ostream& _os;
        ObjectNamer& _namer;
        Module* _ctxt;

    public:
        Context(std::ostream& os, Module* ctxt) :
            _os(os),
            _namer(ctxt->design().namer()),
            _ctxt(ctxt)
        { }

        template<typename C>
        std::string name(C c) {
            return _namer.getName(c, _ctxt);
        }

        template<typename T>
        Context& operator<<(T t) {
            _os << t;
            return *this;
        }

        std::string primBlockName(Block* b) {
            return _namer.primBlockName(b);
        }

        DEF_GET_NP(namer);
    };


    class Printer;
    typedef llpm::PriorityCollection<Block, Printer> PCollection;

    class Printer: public PCHandler<Block> {
    protected:
        Printer() { }

    public:
        virtual ~Printer() { }

        virtual bool handles(Block*) const = 0;
        virtual void print(Context& ctxt, Block* c) const = 0;
    };

private:
    Design& _design;
    PCollection _printers;

    void addDefaultPrinters();

public:
    VerilogSynthesizer(Design& design);
    virtual ~VerilogSynthesizer() { }

    DEF_GET_NP(printers);

    void writeModule(std::ostream& os, Module* mod);
    void write(std::ostream& os);

    void print(Context& ctxt, Block* b);
};

} // namespace llpm

#endif // __LLPM_VERILOG_SYNTHESIZE_HPP__
