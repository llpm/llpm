#include "print.hpp"

#include <llpm/module.hpp>
#include <util/misc.hpp>
#include <backends/graphviz/graphviz.hpp>

#include <boost/format.hpp>

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

void TextPrinterPass::runInternal(Module* mod) {
    unsigned ctr = _counter[mod];
    std::string fn = str(boost::format("%1%_%2%%3$03u.txt")
                            % mod->name()
                            % _name
                            % ctr);

    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        return;
    auto f =_design.workingDir()->create(fn);
    auto fd = f->openFile("w");
    for (auto p: conns->sourcesRaw()) {
        fprintf(fd, "%s (%u:%s) -> %s (%u:%s)\n",
                p.second->owner()->globalName().c_str(),
                p.second->num(),
                p.second->name().c_str(),
                p.first->owner()->globalName().c_str(),
                p.first->num(),
                p.first->name().c_str());
    }
    f->close();
}

void StatsPrinterPass::runInternal(Module* mod) {
    ConnectionDB* conns = mod->conns();
    if (conns == NULL)
        return;

    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    for (auto b: blocks) {
        _typeCounters[typeid(*b).name()] += 1;
    }
}

void StatsPrinterPass::finalize() {
    std::string fn = str(boost::format("stats%1%.csv")
                            % _name);
    auto f =_design.workingDir()->create(fn);
    auto fd = f->openFile("w");
    for (auto p: _typeCounters) {
        fprintf(fd, "%s, %lu\n",
                cpp_demangle(p.first).c_str(),
                p.second);
    }
    f->close();
}

} // namespace llpm
