#ifndef __UTIL_GRAPH_HPP__
#define __UTIL_GRAPH_HPP__

#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <set>

namespace llpm {

bool dfs_cycle(Block* b, std::set<Block*>& path,
               const ConnectionDB* conns);

};

#endif //__UTIL_GRAPH_HPP__
