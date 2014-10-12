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
    class Printer;
    typedef llpm::PriorityCollection<Block, Printer> PCollection;

    class Printer: public PCHandler<Block> {
    protected:
        Printer() { }

    public:
        virtual ~Printer() { }

        virtual bool handles(Block*) const = 0;
        virtual void print(std::ostream& os, ObjectNamer& namer, Block* c, Module* ctxt) const = 0;
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

    void print(std::ostream& os, Block* b, Module* mod);
};

} // namespace llpm

#endif // __LLPM_VERILOG_SYNTHESIZE_HPP__
