#include "path.hpp"
#include <llpm/design.hpp>
#include <llpm/module.hpp>

using namespace std;

namespace llpm {

bool Path::missingFront() const {
    return _path.size() > 0 &&
           _path.front().source() == nullptr;
}

bool Path::missingBack() const {
    return _path.size() > 0 &&
           _path.back().sink() == nullptr;
}

#define InvalidArgAssert(TERM, MSG) \
    if (!(TERM)) \
        throw InvalidArgument(MSG);

void Path::push_back(Connection c) {
    InvalidArgAssert(c.valid(),
                     "Connection must be complete");
    InvalidArgAssert(!missingBack(),
                     "Cannot append connection if missing back IP");
    _path.push_back(c);
}

void Path::push_back(OutputPort* op) {
    InvalidArgAssert(op != nullptr,
                     "Cannot append NULL");
    InvalidArgAssert(!missingBack(),
                     "Cannot append OP if missing back IP");
    _path.push_back(Connection(op, nullptr));
}

void Path::push_back(InputPort* ip) {
    InvalidArgAssert(ip != nullptr,
                     "Cannot append NULL");

    if (missingBack())
        _path.back() = Connection(_path.back().source(), ip);
    else
        _path.push_back(Connection(nullptr, ip));
}

void Path::push_front(Connection c) {
    InvalidArgAssert(c.valid(),
                     "Connection must be complete");
    InvalidArgAssert(!missingFront(),
                     "Cannot append connection if missing front OP");
    _path.push_front(c);
}

void Path::push_front(OutputPort* op) {
    InvalidArgAssert(op != nullptr,
                     "Cannot prepend NULL");

    if (missingFront())
        _path.front() = Connection(op, _path.front().sink());
    else
        _path.push_back(Connection(nullptr, op));
}
void Path::push_front(InputPort* ip) {
    InvalidArgAssert(ip != nullptr,
                     "Cannot prepend NULL");
    InvalidArgAssert(!missingFront(),
                     "Cannot prepend IP if missing front OP");
    _path.push_back(Connection(nullptr, ip));
}

bool Path::isValid() const {
    for (auto i=_path.begin(); i!=_path.end(); i++) {
        // Check for invalid NULL ptrs
        if (i==_path.begin()) {
            if (i->sink() == nullptr)
                return false;
        } else if ((i+1)==_path.end()) {
            if (i->source() == nullptr)
                return false;
        } else {
            if (!i->valid())
                return false;
        }

        // Check validity of intermediate blocks
        if (i!=_path.begin()) {
            auto last = *(i-1);
            auto ip = last.sink();
            auto op = i->source();
            unsigned foundIPs = 0;
            auto dr = op->deps();
            for (auto dep: dr.inputs) {
                if (dep == ip)
                    foundIPs += 1;
            }
            if (foundIPs != 1)
                return false;
        }
    }
    return true;
}

Latency Path::latency(Backend* backend) const {
    Latency l(Time::ns(0),
              PipelineDepth::Fixed(0));

    for (auto i=_path.begin(); i!=_path.end(); i++) {
        // Get latency of intervening block
        if (i!=_path.begin()) {
            auto last = *(i-1);
            auto ip = last.sink();
            auto op = i->source();
            l = l + backend->latency(ip, op);
        }
        
        if (i->valid()) {
            l = l + backend->latency(*i);
        }
    }
    return l;
}

} // namespace llpm
