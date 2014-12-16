#ifndef __LIBRARIES_CORE_INTERFACE_HPP__
#define __LIBRARIES_CORE_INTERFACE_HPP__

#include <llpm/block.hpp>

#include <boost/format.hpp>

namespace llpm {

class Interface {
    InputPort _cin;
    OutputPort _cout;

public:
    Interface(Block* owner,
              llvm::Type* inpType,
              llvm::Type* outType,
              std::string name = "") :
        _cin(owner, inpType, name + "_cin"),
        _cout(owner, outType, name + "_cout")
    { }

    DEF_GET(cin);
    DEF_GET(cout);
};

class InterfaceMultiplexer : public Block {
    OutputPort _sin;
    InputPort  _sout;

    std::vector<Interface*> _interfaces;

public:
    InterfaceMultiplexer(llvm::Type* inpType,
                         llvm::Type* outType,
                         std::string name) :
        _sin(this, inpType, name + "_sin"),
        _sout(this, outType, name = "_sout")
    {
        this->name(name);
    }

    DEF_GET(sin);
    DEF_GET(sout);

    Interface* createInterface() {
        std::string iname = str(boost::format("%1%_iface%2%")
                                % name()
                                % _interfaces.size());
        Interface* i = new Interface(this, _sin.type(), _sout.type(), iname);
        _interfaces.push_back(i);
        return i;
    }
};

};

#endif // __LIBRARIES_CORE_INTERFACE_HPP__

