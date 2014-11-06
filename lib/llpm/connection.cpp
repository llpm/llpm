#include "connection.hpp"
#include <llpm/block.hpp>
#include <boost/foreach.hpp>

namespace llpm {

void ConnectionDB::registerBlock(Block* block) {
    if (_blockUseCounts.find(block) == _blockUseCounts.end()) {
        _newBlocks.insert(block);
        block->module(_module);
    }

    uint64_t& count = _blockUseCounts[block];
    count += 1;
}

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

// TODO -- This is slow. Rewrite w/ an index
bool ConnectionDB::find(const InputPort* ip, Connection& cOut) const {
    for(auto&& c: _connections) {
        const InputPort* sink = c.sink();
        if (sink == ip) {
            cOut = c;
            return true;
        }
    }

    return false;
}

// TODO -- This is slow. Rewrite w/ an index
void ConnectionDB::find(const OutputPort* op, std::vector<Connection>& out) const {
    for(auto&& c: _connections) {
        const OutputPort* source = c.source();
        if (source == op)
            out.push_back(c);
    }
}

// TODO -- This is slow. Rewrite w/ an index
void ConnectionDB::find(Block* b, std::vector<Connection>& out) const {
    for(auto&& c: _connections) {
        if (c.source()->owner() == b ||
            c.sink()->owner() == b)
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

    bool blacklisted = 
        _blacklist.count(o->owner()) > 0 || 
        _blacklist.count(i->owner()) > 0;
    Connection c(o, i, blacklisted);
    _connections.insert(c);
    registerBlock(o->owner());
    registerBlock(i->owner());

#ifndef LLPM_NO_DEBUG
    {
        Connection cf;
        auto found = this->find(i, cf);
        assert(found);
        assert(cf.sink() == i);
        assert(cf.source() == o);
    }
#endif
}

void ConnectionDB::disconnect(OutputPort* o, InputPort* i) {
    assert(o != NULL);
    assert(i != NULL);
    auto f = _connections.find(Connection(o, i, false));
    if (f != _connections.end())
        _connections.erase(f);
    deregisterBlock(o->owner());
    deregisterBlock(i->owner());

#ifndef LLPM_NO_DEBUG
    {
        Connection c;
        auto found = this->find(i, c);
        assert(!found);
    }
#endif
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

void ConnectionDB::removeBlock(Block* b) {
    std::vector<Connection> conns;
    this->find(b, conns);
    for (auto&& c: conns) {
        this->disconnect(c);
    }
}

} // namespace llpm

