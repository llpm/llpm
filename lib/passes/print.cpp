#include "print.hpp"

#include <llpm/module.hpp>
#include <boost/format.hpp>
#include <backends/graphviz/graphviz.hpp>

using namespace std;

namespace llpm {

void GVPrinterPass::runInternal(Module* mod) {
    unsigned ctr = _counter[mod];
    std::string fn = str(boost::format("%1%_%2%%3$03u.gv")
                            % mod->name()
                            % _name
                            % ctr);

    auto f =_design.workingDir()->create(fn);
    _design.gv()->writeModule(
        f,
        mod,
        false,
        _name);
    _counter[mod]++;
    f->close();
}

} // namespace llpm
