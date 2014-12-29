#ifndef __LIBRARIES_CORE_INTERFACE_HPP__
#define __LIBRARIES_CORE_INTERFACE_HPP__

#include <llpm/block.hpp>

#include <boost/format.hpp>

namespace llpm {

class Interface {
    InputPort _din;
    OutputPort _dout;
    bool _server;
    std::string _name;

public:
    Interface(Block* owner,
              llvm::Type* inpType,
              llvm::Type* outType,
              bool server,
              std::string name = "") :
        _din(owner, inpType, name + (server ? "_req" : "_resp")),
        _dout(owner, outType, name + (server ? "_resp" : "_req")),
        _server(server),
        _name(name)
    {
        owner->defineInterface(this);
    }

    DEF_GET(din);
    DEF_GET(dout);
    DEF_GET_NP(server);
    DEF_GET_NP(name);

    Block* owner() const {
        return _din.owner();
    }

    const Port* req() const {
        if (_server)
            return &_din;
        else
            return &_dout;
    }

    const Port* resp() const {
        if (_server)
            return &_dout;
        else
            return &_din;
    }

    Port* req() {
        if (_server)
            return &_din;
        else
            return &_dout;
    }

    Port* resp() {
        if (_server)
            return &_dout;
        else
            return &_din;
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

    std::vector<Interface*> _servers;
    std::vector<InputPort*> _clientDinArr;

public:
    InterfaceMultiplexer(Interface* iface) :
        _client(this, iface->respType(), iface->reqType(), false,
                iface->name() + "_mux_client")
    {
        if (!iface->server()) {
            throw InvalidArgument(
                "Can only multiplex a server interface!");
        }
        this->name(iface->name() + "mux");
        _clientDinArr = {_client.din()};
    }

    DEF_GET(client);
    DEF_ARRAY_GET(servers);

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(std::find(outputs().begin(), outputs().end(), op)
               != outputs().end());
        if (op == _client.dout())
            return DependenceRule(DependenceRule::Custom,
                                  DependenceRule::Maybe);
        else
            return DependenceRule(DependenceRule::AND,
                                  DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>& deps(const OutputPort* op) const {
        assert(std::find(outputs().begin(), outputs().end(), op)
               != outputs().end());
        if (op == _client.dout())
            return inputs();
        else
            return _clientDinArr;
    }

    Interface* createServer() {
        std::string iname = str(boost::format("%1%_iface%2%")
                                % name()
                                % _interfaces.size());
        Interface* i =
            new Interface(this,
                          _client.reqType(),
                          _client.respType(), 
                          true,
                          iname);
        _servers.push_back(i);
        return i;
    }

    virtual bool hasState() const {
        return false;
    }

    virtual bool refinable() const {
        return true;
    }

    virtual bool refine(ConnectionDB& conns) const;
};

};

#endif // __LIBRARIES_CORE_INTERFACE_HPP__

