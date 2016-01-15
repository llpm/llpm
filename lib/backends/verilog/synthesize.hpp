#pragma once

#include <util/files.hpp>

#include <llpm/design.hpp>
#include <llpm/module.hpp>
#include <llpm/control_region.hpp>
#include <refinery/refinery.hpp>
#include <frontends/llvm/objects.hpp>
#include <synthesis/object_namer.hpp>
#include <refinery/priority_collection.hpp>
#include <backends/backend.hpp>

// fwd def
namespace llvm {
    class Module;
}

namespace llpm {

// Mo fwd. defs., mo idiocy
class WrapLLPMMModule;

class VerilogSynthesizer : public Backend {
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
            _ctxt(ctxt)
        { }

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
        std::string name(C c, bool io = false) {
            return _namer.getName(c, _ctxt, io);
        }

        template<typename T>
        Context& operator<<(T t) {
            _os << t;
            return *this;
        }

        std::string primBlockName(Block* b) {
            return _namer.primBlockName(b, _ctxt);
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
        virtual bool customLID() const {
            return false;
        }
        virtual bool alwaysWriteValid(Port*) const {
            return false;
        }
        virtual bool alwaysWriteBP(Port*) const {
            return false;
        }
    };

    void writeIO(Context&);
    void writeCRControl(Context&);
    void writeLocalIOControl(Context&);
    void writeBlocks(Context&);

private:
    PCollection _printers;
    BaseLibraryStopCondition _stops;
    bool _copiedExt;

    void addDefaultPrinters();
    void addStops();

    void writeWrapper(FileSet& dir,
                      WrapLLPMMModule* mod,
                      std::set<FileSet::File*>& files);

    static InputPort* findSink(const ConnectionDB*, const OutputPort*);

public:
    VerilogSynthesizer(Design& design);
    virtual ~VerilogSynthesizer() { }

    DEF_GET_NP(printers);

    virtual void writeModule(FileSet& dir,
                             Module* mod,
                             std::set<FileSet::File*>& files);

    void print(Context& ctxt, Block* b);

    virtual bool blockIsPrimitive(Block* b);
    virtual Refinery::StopCondition* primitiveStops() {
        return &_stops;
    }

    virtual bool synchronous() const {
        return true;
    }
};

} // namespace llpm

