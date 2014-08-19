#ifndef __LLPM_CONNECTION_HPP__
#define __LLPM_CONNECTION_HPP__

#include <llpm/ports.hpp>
#include <llpm/exceptions.hpp>
#include <util/macros.hpp>

#include <set>
#include <map>

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

    Connection() :
        _source(NULL),
        _sink(NULL)
    { }

    bool valid() const {
        return _source != NULL && _sink != NULL;
    }

    DEF_GET_NP(source);
    DEF_SET_NONULL(source);
    DEF_GET_NP(sink);
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
    std::map<const InputPort*, vector<InputPort*> > _inputRewrites;
    std::map<const OutputPort*, OutputPort*> _outputRewrites;

public:
    ConnectionDB(Module* m) :
        _module(m)
    { }

    DEF_GET(module);

    void connect(OutputPort* o, InputPort* i);
    void disconnect(OutputPort* o, InputPort* i);
    void disconnect(Connection c) {
        disconnect(c.source(), c.sink());
    }

    size_t numConnections() {
        return _connections.size();
    }

    void findSinks(const OutputPort* op, std::vector<InputPort*>& out) const;
    OutputPort* findSource(const InputPort* ip) const;

    bool find(const InputPort* ip, Connection& c);
    void find(const OutputPort* op, std::vector<Connection>& out);

    void update(const ConnectionDB& newdb);
    
    /*****
     * Remaps an input port or output port to point to new
     * locations. If the input or output ports are unknown to this
     * connection database, the remaps are stored so they may be
     * processed later by an update or a connect.
     */
    void remap(const InputPort* origPort, const vector<InputPort*>& newPorts);
    void remap(const OutputPort* origPort, OutputPort* newPort);
};

} // namespace llpm

#endif // __LLPM_CONNECTION_HPP__
