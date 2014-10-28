#ifndef __LLPM_VERILOG_SYNTHESIZE_HPP__
#define __LLPM_VERILOG_SYNTHESIZE_HPP__

#include <util/files.hpp>

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <refinery/refinery.hpp>
#include <frontends/llvm/objects.hpp>
#include <synthesis/object_namer.hpp>
#include <refinery/priority_collection.hpp>
#include <synthesis/schedule.hpp>

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

        std::map<OutputPort*, std::string> _mapping;

    public:
        Context(std::ostream& os, Module* ctxt) :
            _os(os),
            _namer(ctxt->design().namer()),
            _ctxt(ctxt),
            layer(NULL)
        { }

        StaticRegion::Layer* layer;
        unsigned layerNum;
        StaticRegion* region;

        void updateMapping(OutputPort* op, std::string name) {
            _mapping[op] = name;
        }

        const std::string& findMapping(OutputPort* op) {
            assert(op != NULL);
            std::string& n = _mapping[op];
            if (n == "")
                throw InvalidArgument("Port does not have a mapping!");
            return n;
        }

        void removeMapping(OutputPort* op) {
            assert(op != NULL);
            auto f = _mapping.find(op);
            if (f == _mapping.end())
                throw InvalidArgument("Cannot remove mapping that does not exist!");
            _mapping.erase(f);
        }

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
        Module* module() const {
            return _ctxt;
        }
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

    void writeModule(FileSet::File* fn, Module* mod);
    void writeModule(std::ostream& os, Module* mod);
    void write(std::ostream& os);

    void print(Context& ctxt, Block* b);
};

} // namespace llpm

#endif // __LLPM_VERILOG_SYNTHESIZE_HPP__
