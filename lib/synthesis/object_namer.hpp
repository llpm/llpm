#ifndef __LLPM_SYNTHESIS_OBJECT_NAMER_HPP__
#define __LLPM_SYNTHESIS_OBJECT_NAMER_HPP__

#include <string>
#include <map>
#include <set>

#include <boost/noncopyable.hpp>

namespace llpm {

// Fwd defs are not fwd thinking
class Block;
class Module;
class Port;
class InputPort;
class OutputPort;

class ObjectNamer : boost::noncopyable {
    uint64_t anonBlockCounter;
    std::set< std::pair<Module*, std::string> > _existingNames;
    std::map<Block*, std::string> _blockNames;
    std::map<Port*, std::string> _portNames;

public:
    ObjectNamer() :
        anonBlockCounter(0)
    { }

    virtual ~ObjectNamer() { }

    virtual std::string primBlockName(Block* b, Module* ctxt);
    virtual std::string getName(Block* b, Module* ctxt, bool ignore=false);
    virtual std::string getName(InputPort* p, Module* ctxt, bool io=false);
    virtual std::string getName(OutputPort* p, Module* ctxt, bool io=false);

    virtual void assignName(Port* p, Module* ctxt, std::string name);

    virtual void reserveName(std::string name, Module* ctxt) {
        _existingNames.insert(make_pair(ctxt, name));
    }
};

}

#endif // __LLPM_SYNTHESIS_OBJECT_NAMER_HPP__
