#ifndef __ANALYSIS_GRAPH_IMPL_HPP__
#define __ANALYSIS_GRAPH_IMPL_HPP__

#include "graph.hpp"

#include <set>
#include <deque>

namespace llpm {

template<typename Path>
Terminate Visitor<Path>::next(
        const ConnectionDB*,
        const PathTy& path,
        std::vector<const typename PathTy::SrcPortTy*>& out)
{
    _visits += 1;
    // if ((_visits % 25000) == 0 && _visits > 0) {
        // printf("Graph visits: %lu / %lu\n",
               // _visits, conns->size());
    // }
    Block* b = path.endPort()->owner();
    std::set<const typename PathTy::SrcPortTy*> setOut;
    b->deps(path.endPort(), setOut);
    out.insert(out.end(), setOut.begin(), setOut.end());
    return Continue;
}

template<typename Path>
Terminate Visitor<Path>::pathEnd(const ConnectionDB*,
                                 const PathTy&) {
    return Continue;
}

template<typename SrcPort,
         typename DstPort>
bool Path<SrcPort, DstPort>::contains(Block* b) const {
    for (const auto& pp: _path) {
        if (pp.first->owner() == b ||
            pp.second->owner() == b)
            return true;
    }
    return false;
}

template<typename SrcPort,
         typename DstPort>
bool Path<SrcPort, DstPort>::hasCycle() const {
    std::set< std::pair<const SrcPort*, const DstPort*> > seen;
    for (const auto& pp: _path) {
        if (seen.count(pp) != 0)
            return true;
        seen.insert(pp);
    }
    return false;
}

template<typename SrcPort,
         typename DstPort>
std::vector< std::pair<const SrcPort*, const DstPort*> >
Path<SrcPort, DstPort>::extractCycle() const {
    std::set< std::pair<const SrcPort*, const DstPort*> > seen;
    std::pair<const SrcPort*, const DstPort*> crux;
    for (const auto& pp: _path) {
        if (seen.count(pp) != 0) {
            crux = pp;
            break;
        }
        seen.insert(pp);
    }

    bool hitCrux = false;
    std::vector< std::pair<const SrcPort*, const DstPort*> > cycle;
    for (const auto& pp: _path) {
        if (!hitCrux) {
            if (pp == crux)
                hitCrux = true;
        } else {
            cycle.push_back(pp);
            if (pp == crux)
                break;
        }
    }

    return cycle; 
}

template<typename SrcPort,
         typename DstPort>
void Path<SrcPort, DstPort>::print() const {
    for (const auto& pp: _path) {
        printf("%p owner: %p %s -- %p owner %p %s\n",
               pp.first, pp.first->owner(),
               pp.first->owner()->name().c_str(),
               pp.second, pp.second->owner(),
               pp.second->owner()->name().c_str());
    }
}

template<typename PathTy>
struct Next {
    PathTy path;
    bool pop;

    static Next newPop() {
        Next n = { PathTy(), true };
        return n;
    }

    static Next newPath(PathTy p) {
        Next n = { p, false };
        return n;
    }
};

template<typename Visitor,
         const SearchAlgo Algo>
template<typename Container>
void GraphSearch<Visitor, Algo>::go(const Container& init) {
    std::set<PathTy> seen;
    std::deque<Next<PathTy>> queue;
    for (SrcPortTy* src: init) {
        std::vector<DstPortTy*> dsts;
        _conns->find(src, dsts);

        for (DstPortTy* dst: dsts) {
            PathTy p(src, dst);
            queue.push_back(Next<PathTy>::newPath(p));
        }
    }

    Terminate terminate = Continue;
    while (terminate == Continue &&
           !queue.empty()) {

        auto current = queue.front();
        queue.pop_front();
        
        Terminate t;
        if (current.pop) {
            t = _visitor.pop(_conns);
        } else {
            t = _visitor.visit(_conns, current.path);
            if (Algo == DFS) {
                queue.push_front(Next<PathTy>::newPop());
            }
            if (t == Continue) {
                std::vector<SrcPortTy*> next;
                t = _visitor.next(_conns, current.path, next);
                if (t == Continue) {
                    bool foundDst = false;
                    for (SrcPortTy* src: next) {
                        std::vector<DstPortTy*> dsts;
                        _conns->find(src, dsts);
                        for (DstPortTy* dst: dsts) {
                            foundDst = true;
                            PathTy p = current.path.push(src, dst);
                            if (seen.count(p))
                                continue;
                            seen.insert(p);
                            switch (Algo) {
                            case DFS:
                                queue.push_front(Next<PathTy>::newPath(p));
                                break;
                            case BFS:
                                queue.push_back(Next<PathTy>::newPath(p));
                            }
                        }
                    }
                    if (!foundDst) {
                        t = _visitor.pathEnd(_conns, current.path);
                    }
                }
            } 
            if (!current.pop && Algo == BFS) { 
                queue.push_back(Next<PathTy>::newPop());
            }
        }

        if (t == TerminatePath) {
            t = _visitor.pathEnd(_conns, current.path);
        } else if (t == TerminateSearch) {
            terminate = TerminateSearch;
        }
    }
}

} // namespace llpm

#endif // __ANALYSIS_GRAPH_IMPL_HPP__
