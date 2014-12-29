#ifndef __ANALYSIS_GRAPH_QUERIES_HPP__
#define __ANALYSIS_GRAPH_QUERIES_HPP__

#include <llpm/connection.hpp>

namespace llpm {
namespace queries {

bool BlockCycleExists(const ConnectionDB* conns,
                      std::vector<OutputPort*> init);

void FindDominators(const ConnectionDB* conns,
                    Block* b,
                    std::set<Block*>& dominators);

// Does this interface potentially deliver responses in the wrong order?
bool CouldReorderTokens(Interface*);

};
};

#endif // __ANALYSIS_GRAPH_QUERIES_HPP__

