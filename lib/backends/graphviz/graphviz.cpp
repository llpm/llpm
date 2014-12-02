#include "graphviz.hpp"

#include <libraries/synthesis/pipeline.hpp>
#include <util/misc.hpp>
#include <util/llvm_type.hpp>
#include <synthesis/schedule.hpp>
#include <synthesis/pipeline.hpp>

#include <boost/format.hpp>

namespace llpm {

void GraphvizOutput::writeModule(FileSet::File* fn, Module* mod) {
    this->writeModule(fn->openStream(), mod);
}

static std::string attrs(const std::map<std::string, std::string>& a) {
    std::string rc = "";
    for (auto p: a) {
        if (rc.size() > 0)
            rc = rc + ",";
        rc = rc + p.first + "=" + p.second;
    }

    return rc;
}

static std::string attrs(ObjectNamer& namer, Block* b) {
    std::map<std::string, std::string> a;
    if (dynamic_cast<PipelineRegister*>(b)) {
        a["shape"] = "rectangle";
        a["label"] = "reg";
    } else {
        a["shape"] = "component";
        a["label"]="\"" + namer.getName(b, b->module()) + "\\n" 
            + b->print() + "\\n"
            + cpp_demangle(typeid(*b).name()) + "\"";
    }

    return attrs(a);
}

static std::string attrs(ObjectNamer& namer, OutputPort* op, InputPort* ip) {
    std::map<std::string, std::string> a;

    auto opName = op->name();
    if (opName == "")
        opName = str(boost::format("%1%") % op->owner()->outputNum(op));
    auto ipName = ip->name();
    if (ipName == "")
        ipName = str(boost::format("%1%") % ip->owner()->inputNum(ip));
    a["label"] = "\"" + opName + "\\n" 
        + ipName + "\\n"
        + typestr(op->type()) + "\"";
    a["fontsize"] = "8.0";

    return attrs(a);
}

void GraphvizOutput::writeModule(std::ostream& os, Module* mod) {
    ObjectNamer& namer = mod->design().namer();

    ConnectionDB* conns = mod->conns();
    assert(conns != NULL);

    os << "digraph " << mod->name() << " {\n";
    Schedule* sched = mod->schedule();
    sched->buildSchedule();

    for (auto sr: sched->regions()) {
        os << boost::format("    subgraph cluster_sr%1% {\n"
                            "        color=black;\n")
                                % sr->id();
        for (Block* b: sr->blocks()) {
            os << "        " << namer.getName(b, mod)
               << "[" << attrs(namer, b) << "];\n";
        }
        os << "    }\n";
    }

    const set<Connection>& rawConns = conns->raw();
    for (auto conn: rawConns) {
        auto op = conn.source();
        auto ip = conn.sink();
        os << "    " << namer.getName(op->owner(), mod) << " -> "
           << namer.getName(ip->owner(), mod) 
           << "[" << attrs(namer, op, ip) << "];\n";
    }

    os << "}\n";
}

};

