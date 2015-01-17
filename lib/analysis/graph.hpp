#ifndef __ANALYSIS_GRAPH_HPP__
#define __ANALYSIS_GRAPH_HPP__

/***************
 *  Library of graph analyses which operate on LLPM connections in a
 *  ConnectionDB. These searches are _Port_ centric, meaning visitors examine
 *  ports as vertices rather than blocks. The reason is that ports can be
 *  used to access blocks and implicitly define connections, so using them
 *  ensures no information loss without adding more arguments.
 */

#include <llpm/ports.hpp>
#include <llpm/connection.hpp>

#include <vector>

namespace llpm {

// Whether or not search should be continued
enum Terminate {
    Continue,
    TerminatePath,
    TerminateSearch
};

// Although not strictly necessary, clients should write then own visitors
// by extending this struct of one of its children.
template<typename Path>
struct Visitor {
    uint64_t _visits;
    typedef Path PathTy;

    Visitor() :
        _visits(0) { }

    // Visit a vertex in the graph
    Terminate visit(const ConnectionDB*,
                    const PathTy& path) {
        return Continue;
    }

    // When we are done recursively visiting a node, run this. Can be
    // used for the client to maintain a stack
    Terminate pop(const ConnectionDB*) {
        return Continue;
    }

    // Find the src ports exposed by the vertex we are visiting.
    Terminate next(const ConnectionDB*,
                   const PathTy& path, 
                   std::vector<typename PathTy::SrcPortTy*>&);

    // Called when a path will not grow longer -- when a block has no
    // inputs, essentially
    Terminate pathEnd(const ConnectionDB*,
                      const PathTy& path);
};

template<typename SrcPort, typename DstPort>
class Edge : public std::pair<SrcPort*, DstPort*> {
public:
    typedef SrcPort SrcPortTy;
    typedef DstPort DstPortTy;

    Edge() { }

    Edge(SrcPort* src, DstPort* dst):
        std::pair<SrcPort*, DstPort*>(src, dst)
    { }

    Edge push(SrcPort* src, DstPort* dst) const {
        return Edge(src, dst);
    }

    std::pair<SrcPort*, DstPort*> end() const {
        return *this;
    }
    DstPort* endPort() const {
        return this->second;
    }
};

typedef Edge<OutputPort, InputPort> OIEdge;
typedef Edge<InputPort, OutputPort> IOEdge;
typedef Edge<OutputPort, InputPort> SourceSinkEdge;
typedef Edge<InputPort, OutputPort> SinkSourceEdge;
typedef Edge<Port, Port>            GenericEdge;

template<typename SrcPort, typename DstPort>
class VisitPort {
public:
    typedef SrcPort SrcPortTy;
    typedef DstPort DstPortTy;

protected:
    DstPort* _dst;

public:
    VisitPort() { }

    VisitPort(SrcPort*, DstPort* dst):
        _dst(dst)
    { }

    VisitPort push(SrcPort* src, DstPort* dst) const {
        return VisitPort(src, dst);
    }

    DstPort* end() const {
        return _dst;
    }
    DstPort* endPort() const {
        return _dst;
    }

    bool operator==(const VisitPort& e) const {
        return _dst == e._dst;
    }

    bool operator<(const VisitPort& e) const {
        return _dst < e._dst;
    }
};

typedef VisitPort<OutputPort, InputPort> VisitInputPort;
typedef VisitPort<InputPort, OutputPort> VisitOutputPort;

template<typename SrcPort, typename DstPort>
class Path {
    std::vector< std::pair<SrcPort*, DstPort*> > _path;

public:
    typedef SrcPort SrcPortTy;
    typedef DstPort DstPortTy;

    Path() { }

    Path(SrcPort* src, DstPort* dst) {
        _path.push_back(std::make_pair(src, dst));
    }

    Path(const Path* p, SrcPort* src, DstPort* dst) :
        _path(p->_path) {
        _path.push_back(std::make_pair(src, dst));
    }

    Path push(SrcPort* sp, DstPort* dp) const {
        return Path(this, sp, dp);
    }

    std::pair<SrcPort*, DstPort*> end() const {
        return _path.back();
    }
    DstPort* endPort() const {
        return end().second;
    }

    void popEnd() {
        _path.pop_back();
    }

    const std::vector< std::pair<SrcPort*, DstPort*> >& raw() const {
        return _path;
    }

    bool isValid(ConnectionDB*) const;
    bool canBackpressure() const;
    bool canBubble() const;
    bool hasCycle() const;
    bool contains(Port* p) const;
    bool contains(Block* b) const;
    bool contains(Connection c) const;
    unsigned countPipelineDepth() const;

    std::vector< std::pair<SrcPort*, DstPort*> > extractCycle() const;

    bool operator==(const Path& p) const {
        return _path == p._path;
    }

    bool operator<(const Path& p) const {
        return _path < p._path;
    }

    void print() const;
};

typedef Path<OutputPort, InputPort> SourceSinkPath;
typedef Path<InputPort, OutputPort> SinkSourcePath;
typedef Path<Port, Port>            GenericPath;

enum SearchAlgo {
    DFS,
    BFS
};

template<typename Visitor,
         const SearchAlgo Algo>
class GraphSearch {
public:
    typedef Visitor VisitorTy;
    typedef typename VisitorTy::PathTy PathTy;
    typedef typename PathTy::SrcPortTy SrcPortTy;
    typedef typename PathTy::DstPortTy DstPortTy;

protected:
    const ConnectionDB* _conns;
    VisitorTy& _visitor;

public:
    GraphSearch(const ConnectionDB* conns, VisitorTy& visitor) :
        _conns(conns),
        _visitor(visitor)
    { }

    template<typename Container>
    void go(const Container& init);
};

};

#include "graph_impl.hpp"

#endif // __ANALYSIS_GRAPH_HPP__

