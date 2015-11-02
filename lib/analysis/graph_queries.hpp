#ifndef __ANALYSIS_GRAPH_QUERIES_HPP__
#define __ANALYSIS_GRAPH_QUERIES_HPP__

#include <llpm/connection.hpp>
#include <llpm/module.hpp>

namespace llpm {
namespace queries {

bool BlockCycleExists(const ConnectionDB* conns,
                      const std::set<OutputPort*>& init);

void FindDominators(const ConnectionDB* conns,
                    Block* b,
                    std::set<Block*>& dominators);

void FindDominators(const ConnectionDB* conns,
                    const std::vector<InputPort*>& b,
                    std::set<OutputPort*>& dominators);


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

// Finds a cycle in module while ignoring some connections
bool FindCycle(Module*,
               boost::function<bool(Block*)> ignoreBlock,
               std::vector< Connection >& cycle);

// Find all edges and blocks which are driven entirely by Constants
void FindConstants(Module*,
                   std::set<Port*>& edges,
                   std::set<Block*>& blocks);

// Given an input port, find all the outputs upon which it may depend
void FindDependencies(const Module*,
                      InputPort*,
                      std::set<OutputPort*>& deps,
                      DependenceRule& rule);

// Given an output port, find all ports which may depend on it
void FindConsumers(const Module*,
                   OutputPort*,
                   std::set<InputPort*>& consumers,
                   boost::function<bool(Block*)> ignoreBlock);

// Attempt to find a single source for a particular subfield on an input port
OutputPort* FindSubfieldDriver(const Module*,
                               InputPort*,
                               std::vector<unsigned> subfield);

// If a port is driven by a constant, find & return that constant
llvm::Constant* FindConstant(const Module*, Port*);
};
};

#endif // __ANALYSIS_GRAPH_QUERIES_HPP__

