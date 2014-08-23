#include "connection.hpp"
#include <boost/foreach.hpp>

namespace llpm {

// TODO -- This is slow. Rewrite w/ an index
void ConnectionDB::findSinks(const OutputPort* op, std::vector<InputPort*>& out) const
{
    for(auto&& c: _connections) {
        const OutputPort* source = c.source();
        if (source == op)
            out.push_back(c.sink());
    }
}

// TODO -- This is slow. Rewrite w/ an index
OutputPort* ConnectionDB::findSource(const InputPort* ip) const
{
    for(auto&& c: _connections) {
        const InputPort* sink = c.sink();
        if (sink == ip)
            return c.source();
    }

    return NULL;
}

bool ConnectionDB::find(const InputPort* ip, Connection& cOut) {
    for(auto&& c: _connections) {
        const InputPort* sink = c.sink();
        if (sink == ip) {
            cOut = c;
            return true;
        }
    }

    return false;
}

void ConnectionDB::find(const OutputPort* op, std::vector<Connection>& out) {
    for(auto&& c: _connections) {
        const OutputPort* source = c.source();
        if (source == op)
            out.push_back(c);
    }
}

void ConnectionDB::connect(OutputPort* o, InputPort* i) {
    assert(o);
    assert(i);
    if (o->type() != i->type())
        throw TypeError("Ports being connected must have matching types!");

    if (_outputRewrites.find(o) != _outputRewrites.end())
        throw InvalidArgument("The output port being connected has been rewritten!");

    if (_inputRewrites.find(i) != _inputRewrites.end())
        throw InvalidArgument("The input port being connected has been rewritten!");

    _connections.insert(Connection(o, i));
}

void ConnectionDB::disconnect(OutputPort* o, InputPort* i) {
    auto f = _connections.find(Connection(o, i));
    if (f != _connections.end())
        _connections.erase(f);
}

void ConnectionDB::remap(const InputPort* origPort, const vector<InputPort*>& newPorts) {
    _inputRewrites[origPort] = newPorts;

    Connection c;
    if (find(origPort, c)) {
        disconnect(c);
        for(InputPort* ip: newPorts) {
            connect(c.source(), ip);
        }
    }
}

void ConnectionDB::remap(const OutputPort* origPort, OutputPort* newPort) {
    _outputRewrites[origPort] = newPort;

    std::vector<Connection> conns;
    find(origPort, conns);
    for(Connection& c: conns) {
        disconnect(c);
        connect(newPort, c.sink());
    }
}

} // namespace llpm

