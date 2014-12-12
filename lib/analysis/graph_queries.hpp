#ifndef __ANALYSIS_GRAPH_QUERIES_HPP__
#define __ANALYSIS_GRAPH_QUERIES_HPP__

#include <llpm/connection.hpp>

namespace llpm {
namespace queries {

bool BlockCycleExists(const ConnectionDB* conns, vector<OutputPort*> init);

};
};

#endif // __ANALYSIS_GRAPH_QUERIES_HPP__

