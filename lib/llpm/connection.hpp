#ifndef __LLPM_CONNECTION_HPP__
#define __LLPM_CONNECTION_HPP__

#include <llpm/ports.hpp>
#include <llpm/block.hpp>
#include <llpm/exceptions.hpp>
#include <util/macros.hpp>

#include <boost/function.hpp>
#include <set>
#include <map>
#include <unordered_map>

namespace llpm {

// Fwd. defs. Silly rabbit, modern parsing is for kids!
class Interface;

typedef std::pair<InputPort*, OutputPort*> ConnectionPair;
class Connection : public ConnectionPair {
public:
    Connection(OutputPort* source, InputPort* sink) :
        ConnectionPair(sink, source) { }

    Connection(InputPort* sink, OutputPort* source) :
        ConnectionPair(sink, source) { }

    Connection() :
        ConnectionPair(NULL, NULL) { }

    Connection(const ConnectionPair& cp) :
        ConnectionPair(cp) { }

    Connection(const std::pair<OutputPort*, InputPort*>& p) :
        ConnectionPair(p.second, p.first) { }

    bool valid() const {
        return source() != NULL && sink() != NULL;
    }

    OutputPort* source() const {
        return second;
    }
    InputPort* sink() const {
        return first;
    }

    inline Connection& operator=(const ConnectionPair& cp) {
        first = cp.first;
        second = cp.second;
        return *this;
    }
};

// I hate forward declarations
class Module;

class ConnectionDB {
    Module* _module;
    uint64_t _changeCounter;

    // Connection data are stored in this bidirectional map
    typedef std::unordered_map<OutputPort*, std::set<InputPort*> > SinkIdx;
    typedef std::unordered_map<InputPort*, OutputPort*> SourceIdx;
    SinkIdx   _sinkIdx;
    SourceIdx _sourceIdx;

    std::set<BlockP> _blacklist;
    std::map<BlockP, uint64_t> _blockUseCounts;
    std::set<BlockP> _newBlocks;

    void registerBlock(BlockP block);
    void deregisterBlock(BlockP block) {
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
    const uint64_t* counterPtr() const {
        return &_changeCounter;
    }

    const auto& sinksRaw() const {
        return _sinkIdx;
    }

    const auto& sourcesRaw() const {
        return _sourceIdx;
    }

    class ConstIterator : public SourceIdx::const_iterator {
        friend class ConnectionDB;
        ConstIterator(SourceIdx::const_iterator iter) 
            : SourceIdx::const_iterator(iter) { }
    public:
        Connection operator*() const {
            return Connection(
                SourceIdx::const_iterator::operator*());
        }
    };
    ConstIterator begin() const {
        return ConstIterator(_sourceIdx.begin());
    };
    ConstIterator end() const {
        return ConstIterator(_sourceIdx.end());
    };

    void blacklist(BlockP b) {
        _blacklist.insert(b);
        _changeCounter++;
    }

    void deblacklist(BlockP b) {
        _blacklist.erase(b);
        _changeCounter++;
    }

    bool isblacklisted(BlockP b) const {
        return _blacklist.count(b) > 0;
    }

    bool isblacklisted(Block* b) const {
        return _blacklist.count(b->getptr()) > 0;
    }

    bool isInternalDriver(OutputPort* op) const {
        return _blacklist.count(op->ownerP()) > 0;
    }

    void readAndClearNewBlocks(std::set<BlockP>& nb) {
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
                blocks.insert(pr.first.get());
        }
    }

    void filterBlocks(boost::function<bool(Block*)> ignoreBlock,
                      std::set<Block*>& blocks);

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
        auto f = _blockUseCounts.find(b->getptr());
        if (f == _blockUseCounts.end())
            return false;
        return f->second >= 1;
    }

    bool exists(Connection c) const {
        return c.source() == findSource(c.sink());
    }

    bool connected(OutputPort* op, InputPort* ip) const {
        return exists(Connection(op, ip));
    }

    bool connected(InputPort* ip, OutputPort* op) const {
        return exists(Connection(op, ip));
    }

    size_t numConnections() const {
        return _sourceIdx.size();
    }
    size_t size() const {
        return numConnections();
    }

    void findSinks(const OutputPort* op,
                   std::vector<InputPort*>& out) const;
    void findSinks(const OutputPort* op,
                   std::set<InputPort*>& out) const;

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
