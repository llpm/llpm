#include "fork.hpp"

#include <boost/format.hpp>

using namespace std;

namespace llpm {

OutputPort* Fork::createOutput() {
    string name = str(boost::format("out%1%")
                      % _dout.size());
    auto op = new OutputPort(this, _din.type(), name);
    _dout.push_back(op);
    return op;
}

} // namespace llpm
