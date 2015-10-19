#ifndef __LIBRARIES_CORE_INTERFACE_HPP__
#define __LIBRARIES_CORE_INTERFACE_HPP__

#include <llpm/block.hpp>

#include <boost/format.hpp>

namespace llpm {

// Fwd. defs
class InterfaceMultiplexer;

/**
 * Interface defines an input/output port pair. If the interface is a
 * "server" then the expectation is that one token is produced for
 * every input token in response to it. If the interface is not a
 * "server" and is therefore a "client" when the output port produces
 * a token and expects to be given a response token on its input
 * ports.
 */
class Interface {
    InputPort _din;
    OutputPort _dout;
    bool _server;
    std::string _name;
    InterfaceMultiplexer* _multiplexer;

public:
    Interface(Block* owner,
              llvm::Type* inpType,
              llvm::Type* outType,
              bool server,
              std::string name = "") :
        _din(owner, inpType, name + (server ? "_req" : "_resp")),
        _dout(owner, outType, name + (server ? "_resp" : "_req")),
        _server(server),
        _name(name),
        _multiplexer(nullptr)
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

    InterfaceMultiplexer* multiplexer(ConnectionDB& conns);
};

/**
 * The InterfaceMultiplexer allows multiple clients to share a single
 * server interface. There a new of potential strategies to make this
 * work properly and a pass is necessary to implement one of them.
 */
class InterfaceMultiplexer : public Block {
    Interface _client;

    std::vector<std::unique_ptr<Interface>> _servers;
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
    DEF_UNIQ_ARRAY_GET(servers);

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
        _servers.emplace_back(i);
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

