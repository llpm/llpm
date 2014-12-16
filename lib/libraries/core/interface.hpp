#ifndef __LIBRARIES_CORE_INTERFACE_HPP__
#define __LIBRARIES_CORE_INTERFACE_HPP__

#include <llpm/block.hpp>

#include <boost/format.hpp>

namespace llpm {

class Interface {
    InputPort _din;
    OutputPort _dout;
    bool _server;

public:
    Interface(Block* owner,
              llvm::Type* inpType,
              llvm::Type* outType,
              bool server,
              std::string name = "") :
        _din(owner, inpType, name + "_din"),
        _dout(owner, outType, name + "_dout"),
        _server(server)
    { }

    DEF_GET(din);
    DEF_GET(dout);
    DEF_GET(server);

    const Port* req() const {
        if (_server)
            return &_dout;
        else
            return &_din;
    }

    const Port* resp() const {
        if (_server)
            return &_din;
        else
            return &_dout;
    }

    llvm::Type* respType() const {
        return resp()->type();
    }

    llvm::Type* reqType() const {
        return req()->type();
    }
};

class InterfaceMultiplexer : public Block {
    Interface _client;

    std::vector<Interface*> _interfaces;

public:
    InterfaceMultiplexer(llvm::Type* inpType,
                         llvm::Type* outType,
                         std::string name) :
        _client(this, inpType, outType, false, name)
    {
        this->name(name);
    }

    DEF_GET(client);

    Interface* createInterface() {
        std::string iname = str(boost::format("%1%_iface%2%")
                                % name()
                                % _interfaces.size());
        Interface* i =
            new Interface(this,
                          _client.reqType(),
                          _client.respType(), 
                          true,
                          iname);
        _interfaces.push_back(i);
        return i;
    }
};

};

#endif // __LIBRARIES_CORE_INTERFACE_HPP__

