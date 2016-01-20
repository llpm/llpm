#pragma once

#include <llpm/ports.hpp>
#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <llpm/time.hpp>

#include <deque>

namespace llpm {

// Fwd. defs
class Backend;

/**
 * A forward path through the LLPM graph. Specifies ports and blocks.
 */
class Path {
    std::deque<Connection> _path;

    bool missingFront() const;
    bool missingBack() const;

public:
    Path() { }
    Path(const Path& p) :
        _path(p._path)
    { }

    void push_back(Connection);
    void push_back(OutputPort*);
    void push_back(InputPort*);

    void push_front(Connection);
    void push_front(OutputPort*);
    void push_front(InputPort*);

    bool isValid() const;
    bool exists(ConnectionDB*) const;

    Latency latency(Backend*) const;
};

} // namespace llpm
