#ifndef __LLPM_CONNECTION_HPP__
#define __LLPM_CONNECTION_HPP__

#include <llpm/ports.hpp>
#include <llpm/exceptions.hpp>
#include <util/macros.hpp>

#include <set>
#include <map>
#include <unordered_map>

namespace llpm {

// Fwd. defs. Silly rabbit, modern parsing is for kids!
class Interface;

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
    // DEF_SET_NONULL(source);
    DEF_GET_NP(sink);
    // DEF_SET_NONULL(sink);

    bool operator==(const Connection& c) const {
        return (c._sink == this->_sink) 
            && (c._source == this->_source);
    }

    int operator<(const Connection& c) const {
        if (this->_source != c._source)
            return this->_source < c._source;
        return this->_sink < c._sink;
    }
};

// I hate forward declarations
class Module;

class ConnectionDB {
    Module* _module;
    uint64_t _changeCounter;
    std::set<Connection> _connections;
    std::set<Block*> _blacklist;
    std::unordered_map<Block*, uint64_t> _blockUseCounts;
    std::set<Block*> _newBlocks;
    std::map<const InputPort*, std::vector<InputPort*> > _inputRewrites;
    std::map<const OutputPort*, OutputPort*> _outputRewrites;

    void registerBlock(Block* block);
    void deregisterBlock(Block* block) {
        uint64_t& count = _blockUseCounts[block];
        assert(count >= 1);
        count -= 1;
    }

public:
    ConnectionDB(Module* m) :
        _module(m),
        _changeCounter(0)
    { }

    DEF_GET(module);
    DEF_GET_NP(changeCounter);

    const std::set<Connection>& raw() const {
        return _connections;
    }

    void blacklist(Block* b) {
        _blacklist.insert(b);
        _changeCounter++;
    }

    void deblacklist(Block* b) {
        _blacklist.erase(b);
        _changeCounter++;
    }

    void readAndClearNewBlocks(std::set<Block*>& nb) {
        nb.clear();
        nb.swap(_newBlocks);
        assert(_newBlocks.size() == 0);
    }

    void clearNewBlocks() {
        _newBlocks.clear();
    }

    void findAllBlocks(std::set<Block*>& blocks) const {
        for (auto pr: _blockUseCounts) {
            if (pr.second >= 1 && _blacklist.count(pr.first) == 0)
                blocks.insert(pr.first);
        }
    }

    void connect(OutputPort* o, InputPort* i);
    void connect(InputPort* i, OutputPort* o) {
        connect(o, i);
    }
    void connect(Interface*, Interface*);
    void disconnect(OutputPort* o, InputPort* i);
    void disconnect(InputPort* i, OutputPort* o) {
        disconnect(o, i);
    }
    void disconnect(Connection c) {
        disconnect(c.source(), c.sink());
    }
    void disconnect(Interface* a, Interface* b);

    bool createsCycle(Connection c) const;

    bool isUsed(Block* b) const {
        auto f = _blockUseCounts.find(b);
        if (f == _blockUseCounts.end())
            return false;
        return f->second >= 1;
    }

    bool exists(Connection c) const {
        return _connections.count(c) > 0;
    }

    bool connected(OutputPort* op, InputPort* ip) const {
        return exists(Connection(op, ip));
    }

    bool connected(InputPort* ip, OutputPort* op) const {
        return exists(Connection(op, ip));
    }

    size_t numConnections() const {
        return _connections.size();
    }

    void findSinks(const OutputPort* op,
                   std::vector<InputPort*>& out) const;

    OutputPort* findSource(const InputPort* ip) const;
    void find(Block* b, std::vector<Connection>&) const;
    bool find(const InputPort* ip, Connection& c) const;
    void find(const OutputPort* op, std::vector<Connection>& out) const;

    Interface* findClient(Interface*) const;

    void find(const InputPort* ip, std::vector<OutputPort*>& out) const {
        Connection c;
        if (find(ip, c))
            out.push_back(c.source());
    }
    void find(const OutputPort* ip, std::vector<InputPort*>& out) const {
        std::vector<Connection> ret;
        find(ip, ret);
        for (auto&& c: ret) {
            out.push_back(c.sink());
        }
    }

    void removeBlock(Block* b);

    void update(const ConnectionDB& newdb);
    
    /*****
     * Remaps an input port or output port to point to new
     * locations. If the input or output ports are unknown to this
     * connection database, the remaps are stored so they may be
     * processed later by an update or a connect.
     */
    void remap(const InputPort* origPort, const std::vector<InputPort*>& newPorts);
    void remap(const InputPort* origPort, InputPort* newPort) {
        std::vector<InputPort*> newPorts = {newPort};
        remap(origPort, newPorts);
    }
    void remap(const OutputPort* origPort, OutputPort* newPort);
    void remap(const Interface*, Interface*);
};

} // namespace llpm

#endif // __LLPM_CONNECTION_HPP__
