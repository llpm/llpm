#ifndef __LLPM_CONNECTION_HPP__
#define __LLPM_CONNECTION_HPP__

#include <llpm/ports.hpp>
#include <llpm/exceptions.hpp>
#include <util/macros.hpp>

#include <set>

using namespace std;

namespace llpm {

class Connection {
    OutputPort* _source;
    InputPort* _sink;

public:
    Connection(OutputPort* source, InputPort* sink) :
        _source(source),
        _sink(sink)
    { }

    Connection() { }

    DEF_GET(source);
    DEF_SET_NONULL(source);
    DEF_GET(sink);
    DEF_SET_NONULL(sink);

    bool operator==(const Connection& c) const {
        return c._sink == this->_sink && c._source == this->_source;
    }

    int operator<(const Connection& c) const {
        return ((uint64_t)c._source ^ (uint64_t)c._sink) -
               ((uint64_t)_source   ^ (uint64_t)_sink);
    }
};

// I hate forward declarations
class Module;

class ConnectionDB {
    Module* _module;
    set<Connection> _connections;

public:
    ConnectionDB(Module* m) :
        _module(m)
    { }

    DEF_GET(module);

    void connect(OutputPort* o, InputPort* i) {
        if (o->type() != i->type())
            throw TypeError("Ports being connected must have matching types!");
        _connections.insert(Connection(o, i));
    }

    void disconnect(OutputPort* o, InputPort* i) {
        auto f = _connections.find(Connection(o, i));
        if (f != _connections.end())
            _connections.erase(f);
    }

    size_t numConnections() {
        return _connections.size();
    }
};

} // namespace llpm

#endif // __LLPM_CONNECTION_HPP__
