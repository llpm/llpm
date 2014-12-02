#include "graph.hpp"

namespace llpm {

bool dfs_cycle(Block* b, set<Block*>& path, const ConnectionDB* conns) {
    path.insert(b);
    for (OutputPort* source: b->outputs()) {
        vector<InputPort*> sinks;
        conns->findSinks(source, sinks);
        for (InputPort* sink: sinks) {
            Block* owner = sink->owner();
            if (path.count(owner) > 0)
                return true;
            if (dfs_cycle(owner, path, conns))
                return true;
        }
    }
    path.erase(b);
    return false;
}

}
