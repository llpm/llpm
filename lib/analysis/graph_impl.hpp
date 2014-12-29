#ifndef __ANALYSIS_GRAPH_IMPL_HPP__
#define __ANALYSIS_GRAPH_IMPL_HPP__

#include "graph.hpp"

#include <set>
#include <deque>

namespace llpm {

template<typename Path>
Terminate Visitor<Path>::next(const ConnectionDB*,
                              const PathTy& path, 
                              std::vector<typename PathTy::SrcPortTy*>& out) {
    Block* b = path.endPort()->owner();
    b->ports(out);
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
    std::set< std::pair<SrcPort*, DstPort*> > seen;
    for (const auto& pp: _path) {
        if (seen.count(pp) != 0)
            return true;
        seen.insert(pp);
    }
    return false;
}

template<typename SrcPort,
         typename DstPort>
std::vector< std::pair<SrcPort*, DstPort*> >
Path<SrcPort, DstPort>::extractCycle() const {
    std::set< std::pair<SrcPort*, DstPort*> > seen;
    std::pair<SrcPort*, DstPort*> crux;
    for (const auto& pp: _path) {
        if (seen.count(pp) != 0) {
            crux = pp;
            break;
        }
        seen.insert(pp);
    }

    bool hitCrux = false;
    std::vector< std::pair<SrcPort*, DstPort*> > cycle;
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

template<typename Visitor,
         const SearchAlgo Algo>
template<typename Container>
void GraphSearch<Visitor, Algo>::go(const Container& init) {
    std::set<PathTy> enqueued;
    std::deque<PathTy> queue;
    for (SrcPortTy* src: init) {
        std::vector<DstPortTy*> dsts;
        _conns->find(src, dsts);

        for (DstPortTy* dst: dsts) {
            PathTy p(src, dst);
            if (enqueued.count(p) == 0) {
                queue.push_front(p);
                enqueued.insert(p);
            }
        }
    }

    Terminate terminate = Continue;
    while (terminate == Continue &&
           !queue.empty()) {

        auto current = queue.front();
        queue.pop_front();
        
        Terminate t = _visitor.visit(_conns, current);
        if (t == Continue) {
            std::vector<SrcPortTy*> next;
            _visitor.next(_conns, current, next);

            for (SrcPortTy* src: next) {
                std::vector<DstPortTy*> dsts;
                _conns->find(src, dsts);

                for (DstPortTy* dst: dsts) {
                    PathTy p = current.push(src, dst);
                    if (enqueued.count(p) == 0) {
                        enqueued.insert(p);
                        switch (Algo) {
                        case DFS:
                            queue.push_front(p);
                            break;
                        case BFS:
                            queue.push_back(p);
                        }
                    }
                }
            }
        } else if (t == TerminateSearch) {
            terminate = TerminateSearch;
        } 
    }
}

} // namespace llpm

#endif // __ANALYSIS_GRAPH_IMPL_HPP__
