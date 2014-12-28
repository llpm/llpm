#include "connection.hpp"

#include <llpm/block.hpp>
#include <util/llvm_type.hpp>
#include <libraries/core/interface.hpp>
#include <analysis/graph_queries.hpp>

using namespace std;

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

Interface* ConnectionDB::findClient(Interface* server) const {
    assert(server->server());
    OutputPort* source = findSource(server->din());
    if (source == NULL)
        return NULL;

    for (auto& iface: source->owner()->interfaces()) {
        if (iface->server())
            continue;
        OutputPort* ifDriver = findSource(iface->din());
        if (ifDriver == server->dout())
            return iface;
    }

    return NULL;
}

void ConnectionDB::connect(OutputPort* o, InputPort* i) {
    assert(o);
    assert(i);
    if (o->type() != i->type())
        throw TypeError("Ports being connected must have matching types! "
                        " OP: " + typestr(o->type()) +
                        " IP: " + typestr(i->type()));

    if (_outputRewrites.find(o) != _outputRewrites.end())
        throw InvalidArgument("The output port being connected has been rewritten!");

    if (_inputRewrites.find(i) != _inputRewrites.end())
        throw InvalidArgument("The input port being connected has been rewritten!");

#ifndef LLPM_NO_DEBUG
    {
        assert(findSource(i) == NULL);
    }
#endif

    Connection c(o, i);
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

void ConnectionDB::connect(Interface* a, Interface* b) {
    if (a->server() == b->server()) {
        throw InvalidArgument("Error: when connecting interfaces, one "
                              "must be a server and one a client!");
    }
    connect(a->dout(), b->din());
    connect(a->din(), b->dout());
}

void ConnectionDB::disconnect(OutputPort* o, InputPort* i) {
    assert(o != NULL);
    assert(i != NULL);
    auto f = _connections.find(Connection(o, i));
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

void ConnectionDB::disconnect(Interface* a, Interface* b) {
    disconnect(a->dout(), b->din());
    disconnect(a->din(), b->dout());
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

void ConnectionDB::remap(const Interface* origI, Interface* newI) {
    remap(origI->din(), newI->din());
    remap(origI->dout(), newI->dout());
}

void ConnectionDB::removeBlock(Block* b) {
    std::vector<Connection> conns;
    this->find(b, conns);
    for (auto&& c: conns) {
        this->disconnect(c);
    }
}

void ConnectionDB::update(const ConnectionDB& newdb) {
    for (const Connection& c: newdb._connections) {
        connect(c.source(), c.sink());
    }
    _blacklist.insert(newdb._blacklist.begin(),
                      newdb._blacklist.end());
}

bool ConnectionDB::createsCycle(Connection c) const {
    std::set<Block*> dominators;
    queries::FindDominators(this, c.source()->owner(), dominators);
    return dominators.count(c.sink()->owner()) > 0;
}

} // namespace llpm

