#pragma once

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
class Module;

/**
 * Represents the connection between an output port and an input port.
 */
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

    /// Get the output port driving this connection
    OutputPort* source() const {
        return second;
    }
    /// Get the input port being driven by this connection
    InputPort* sink() const {
        return first;
    }

    inline Connection& operator=(const ConnectionPair& cp) {
        first = cp.first;
        second = cp.second;
        return *this;
    }
};

/**
 * Database of connections. Provides fast access to find out which
 * input ports are driven by a particular output port and which output
 * port drives a particular input port. Blocks contained by this DB
 * are implicitly defined by the ports involved in connections, though
 * the block list is cached.
 *
 * Also, ConnectionDB deals with ownership of the blocks. Internally,
 * the list of blocks are 'intrusive_ptr's to the blocks so the blocks
 * are can be free'd when they are no longer involved in connections.
 */
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
    void deregisterBlock(BlockP block);

public:
    ConnectionDB(Module* m) :
        _module(m),
        _changeCounter(0)
    { }

    DEF_GET_NP(module);
    DEF_GET_NP(changeCounter);

    /**
     * Returns a pointer to a counter which increments every time a
     * change is made to the ConnectionDB. Can be used to as a version
     * number for the DB. Especially useful is one wants to cache
     * queries on the DB.
     */
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

    /**
     * Make this block invisible to clients
     */
    void blacklist(BlockP b) {
        _blacklist.insert(b);
        _changeCounter++;
    }

    /**
     * Make this block visible to clients
     */
    void deblacklist(BlockP b) {
        _blacklist.erase(b);
        _changeCounter++;
    }

    /**
     * Is thie block visible to clients?
     */
    bool isblacklisted(BlockP b) const {
        return _blacklist.count(b) > 0;
    }

    /**
     * Is thie block visible to clients?
     */
    bool isblacklisted(Block* b) const {
        return _blacklist.count(b->getptr()) > 0;
    }

    /**
     * Is thie block visible to clients?
     */
    bool isInternalDriver(const OutputPort* op) const {
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

    /**
     * Return the set of blocks contained by this DB, filtered by the
     * function passed in.
     */
    void filterBlocks(boost::function<bool(Block*)>,
                      std::set<Block*>&);

    void connect(OutputPort* o, InputPort* i);
    void connect(InputPort* i, OutputPort* o) {
        connect(o, i);
    }

    /**
     * Connects one interface to another. If the server interface is already
     * connected to something else, an InterfaceMultiplexer is automatically
     * created or an existing InterfaceMultiplexer is used.
     */
    void connect(Interface*, Interface*);

    /**
     * Connects one interface to a pair of ports (an implicit interface). If
     * the server interface is already connected to something else, an
     * InterfaceMultiplexer is automatically created or an existing
     * InterfaceMultiplexer is used.
     */
    void connect(Interface* srvr, OutputPort* op, InputPort* ip);

    void disconnect(OutputPort* o, InputPort* i);
    void disconnect(InputPort* i, OutputPort* o) {
        disconnect(o, i);
    }
    void disconnect(Connection c) {
        disconnect(c.source(), c.sink());
    }
    void disconnect(Interface* a, Interface* b);

    /**
     * Would adding this connection create a cycle?
     */
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
                   std::vector<const InputPort*>& out) const;
    void findSinks(const OutputPort* op,
                   std::set<const InputPort*>& out) const;
    void findSinks(const OutputPort* op,
                   std::vector<InputPort*>& out) const;
    void findSinks(const OutputPort* op,
                   std::set<InputPort*>& out) const;
    unsigned countSinks(const OutputPort* op) const;

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
    void find(const InputPort* ip, std::vector<const OutputPort*>& out) const {
        Connection c;
        if (find(ip, c))
            out.push_back(c.source());
    }
    void find(const OutputPort* ip, std::vector<const InputPort*>& out) const {
        std::vector<Connection> ret;
        find(ip, ret);
        for (auto&& c: ret) {
            out.push_back(c.sink());
        }
    }

    void removeBlock(Block* b);

    /**
     * Take the contents of the DB passed into this method and put
     * them in this DB.
     */
    void update(const ConnectionDB& newdb);
    
    /**
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

