#ifndef __ANALYSIS_GRAPH_QUERIES_HPP__
#define __ANALYSIS_GRAPH_QUERIES_HPP__

#include <llpm/connection.hpp>
#include <llpm/module.hpp>

namespace llpm {
namespace queries {

bool BlockCycleExists(const ConnectionDB* conns,
                      std::vector<OutputPort*> init);

void FindDominators(const ConnectionDB* conns,
                    Block* b,
                    std::set<Block*>& dominators);


/* Analyze the sources and ordering of tokens between a source and a sink. 
 * - Returns false if the source does not appear to drive the sink.
 * - Output: singleSource iff a token from source is required to observe
 *   a token on sink.
 * - Output: reorderPotenial iff tokens from source may become dis-ordered
 *   by the time they are observed on sink.
 * - Output: cyclic iff there exists a cycle between source and sink
 */
bool TokenOrderAnalysis(Port* source,
                        Port* sink,
                        bool& singleSource,
                        bool& reorderPotential,
                        bool& cyclic);

// Does this interface potentially deliver responses in the wrong order?
bool CouldReorderTokens(Interface*);

// Finds cycles in module. Not guaranteed to find all cycles, but all
// connections involved in cycles will be returned.
void FindCycles(Module*,
                std::vector< std::vector<Connection> >&);

};
};

#endif // __ANALYSIS_GRAPH_QUERIES_HPP__

