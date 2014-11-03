#ifndef __LLPM_CONNECTION_HPP__
#define __LLPM_CONNECTION_HPP__

#include <llpm/ports.hpp>
#include <llpm/exceptions.hpp>
#include <util/macros.hpp>

#include <set>
#include <map>
#include <unordered_map>

using namespace std;

namespace llpm {

class Connection {
    OutputPort* _source;
    InputPort* _sink;
    bool _modIO;

public:
    Connection(OutputPort* source, InputPort* sink, bool moduleIO) :
        _source(source),
        _sink(sink),
        _modIO(moduleIO)
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
    DEF_GET_NP(modIO); 

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
    std::set<Connection> _connections;
    std::set<Block*> _blacklist;
    std::unordered_map<Block*, uint64_t> _blockUseCounts;
    std::set<Block*> _newBlocks;
    std::map<const InputPort*, vector<InputPort*> > _inputRewrites;
    std::map<const OutputPort*, OutputPort*> _outputRewrites;

    void registerBlock(Block* block);
    void deregisterBlock(Block* block) {
        uint64_t& count = _blockUseCounts[block];
        assert(count >= 1);
        count -= 1;
    }

public:
    ConnectionDB(Module* m) :
        _module(m)
    { }

    DEF_GET(module);

    const set<Connection>& raw() const {
        return _connections;
    }

    void blacklist(Block* b) {
        _blacklist.insert(b);
    }

    void readAndClearNewBlocks(std::set<Block*>& nb) {
        nb.clear();
        nb.swap(_newBlocks);
        assert(_newBlocks.size() == 0);
    }

    void clearNewBlocks() {
        _newBlocks.clear();
    }

    void findAllBlocks(set<Block*>& blocks) const {
        for (auto pr: _blockUseCounts) {
            if (pr.second >= 1 && _blacklist.count(pr.first) == 0)
                blocks.insert(pr.first);
        }
    }

    void connect(OutputPort* o, InputPort* i);
    void disconnect(OutputPort* o, InputPort* i);
    void disconnect(Connection c) {
        disconnect(c.source(), c.sink());
    }

    bool isUsed(Block* b) const {
        auto f = _blockUseCounts.find(b);
        if (f == _blockUseCounts.end())
            return false;
        return f->second >= 1;
    }

    bool exists(Connection c) {
        return _connections.count(c) > 0;
    }

    size_t numConnections() {
        return _connections.size();
    }

    void findSinks(const OutputPort* op, std::vector<InputPort*>& out) const;
    OutputPort* findSource(const InputPort* ip) const;
    void find(Block* b, vector<Connection>&) const;
    bool find(const InputPort* ip, Connection& c) const;
    void find(const OutputPort* op, std::vector<Connection>& out)const ;

    void update(const ConnectionDB& newdb);
    
    /*****
     * Remaps an input port or output port to point to new
     * locations. If the input or output ports are unknown to this
     * connection database, the remaps are stored so they may be
     * processed later by an update or a connect.
     */
    void remap(const InputPort* origPort, const vector<InputPort*>& newPorts);
    void remap(const InputPort* origPort, InputPort* newPort) {
        vector<InputPort*> newPorts = {newPort};
        remap(origPort, newPorts);
    }
    void remap(const OutputPort* origPort, OutputPort* newPort);
};

} // namespace llpm

#endif // __LLPM_CONNECTION_HPP__
