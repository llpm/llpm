#include "connection.hpp"

#include <llpm/block.hpp>
#include <util/llvm_type.hpp>
#include <libraries/core/interface.hpp>
#include <analysis/graph_queries.hpp>

using namespace std;

namespace llpm {

void ConnectionDB::registerBlock(BlockP block) {
    if (_blockUseCounts.find(block) == _blockUseCounts.end()) {
        _newBlocks.insert(block);
    }
    block->module(_module);

    uint64_t& count = _blockUseCounts[block];
    count += 1;
    _changeCounter++;
}

void ConnectionDB::findSinks(const OutputPort* op,
                             std::vector<InputPort*>& out) const
{
    const auto& f = _sinkIdx.find((OutputPort*)op);
    if (f != _sinkIdx.end()) {
        for (auto ip: f->second) {
            out.push_back(ip);
        }
    }
}

void ConnectionDB::findSinks(const OutputPort* op,
                             std::set<InputPort*>& out) const
{
    const auto& f = _sinkIdx.find((OutputPort*)op);
    if (f != _sinkIdx.end()) {
        out = f->second;
    }
}

OutputPort* ConnectionDB::findSource(const InputPort* ip) const
{
    auto f = _sourceIdx.find((InputPort*)ip);
    if (f == _sourceIdx.end())
        return NULL;
    return f->second;
}

bool ConnectionDB::find(const InputPort* ip, Connection& cOut) const {
    auto f = _sourceIdx.find((InputPort*)ip);
    if (f == _sourceIdx.end())
        return false;
    cOut = *f;
    return true;
}

void ConnectionDB::find(const OutputPort* op,
                        std::vector<Connection>& out) const {
    const auto& f = _sinkIdx.find((OutputPort*)op);
    if (f != _sinkIdx.end()) {
        for (auto ip: f->second) {
            out.push_back(Connection(f->first, ip));
        }
    }
}

void ConnectionDB::find(Block* b, std::vector<Connection>& out) const {
    for (auto op: b->outputs()) {
        find(op, out);
    }
    for (auto ip: b->inputs()) {
        auto op = findSource(ip);
        if (op && op->owner() != b)
            out.push_back(Connection(op, ip));
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

    registerBlock(o->ownerP());
    registerBlock(i->ownerP());

#ifndef LLPM_NO_DEBUG
    {
        assert(findSource(i) == NULL);
    }
#endif

    _sourceIdx.emplace(i, o);
    _sinkIdx[o].insert(i);
    _changeCounter++;

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
    auto sourceF = _sourceIdx.find(i);
    if (sourceF != _sourceIdx.end() &&
        sourceF->second == o)
        _sourceIdx.erase(sourceF);

    auto sinkF = _sinkIdx.find(o);
    if (sinkF != _sinkIdx.end()) {
        auto& s = sinkF->second;
        auto f = s.find(i);
        if (f != s.end()) {
            s.erase(f);
        }
        if (s.size() == 0)
            _sinkIdx.erase(sinkF);
    }

    _changeCounter++;
    deregisterBlock(o->ownerP());
    deregisterBlock(i->ownerP());

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
    _changeCounter++;

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
    _changeCounter++;

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
    for (const auto& c: newdb._sourceIdx) {
        if (newdb.isblacklisted(c.first->ownerP()) ||
            newdb.isblacklisted(c.second->ownerP()))
            continue;
        connect(c.first, c.second);
    }
    _changeCounter++;
}

bool ConnectionDB::createsCycle(Connection c) const {
    std::set<Block*> dominators;
    queries::FindDominators(this, c.source()->owner(), dominators);
    return dominators.count(c.sink()->owner()) > 0;
}

} // namespace llpm

