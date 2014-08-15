#include "connection.hpp"
#include <boost/foreach.hpp>

namespace llpm {

// TODO -- This is slow. Rewrite w/ an index
void ConnectionDB::findSinks(const OutputPort* op, std::vector<InputPort*>& out) const
{
    BOOST_FOREACH(auto c, _connections) {
        const OutputPort* source = c.source();
        if (source == op)
            out.push_back(c.sink());
    }
}

// TODO -- This is slow. Rewrite w/ an index
OutputPort* ConnectionDB::findSource(const InputPort* ip) const
{
    BOOST_FOREACH(auto c, _connections) {
        const InputPort* sink = c.sink();
        if (sink == ip)
            return c.source();
    }

    return NULL;
}

} // namespace llpm

