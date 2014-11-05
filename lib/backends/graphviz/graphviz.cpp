#include "graphviz.hpp"

#include <libraries/synthesis/pipeline.hpp>

namespace llpm {

void GraphvizOutput::writeModule(FileSet::File* fn, Module* mod) {
    this->writeModule(fn->openStream(), mod);
}

static std::string attrs(ObjectNamer& namer, Block* b) {
    std::map<std::string, std::string> a;
    if (dynamic_cast<PipelineRegister*>(b)) {
        a["shape"] = "rectangle";
        a["label"] = "reg";
    } else {
        a["shape"] = "component";
        a["label"]="\"" + namer.getName(b, b->module()) + "\\n" + typeid(*b).name() + "\"";
    }

    std::string rc = "";
    for (auto p: a) {
        if (rc.size() > 0)
            rc = rc + ",";
        rc = rc + p.first + "=" + p.second;
    }

    return rc;
}

void GraphvizOutput::writeModule(std::ostream& os, Module* mod) {
    ObjectNamer namer;

    ConnectionDB* conns = mod->conns();
    assert(conns != NULL);

    os << "digraph " << mod->name() << " {\n";
    set<Block*> blocks;
    conns->findAllBlocks(blocks);
    for (Block* b: blocks) {
        os << "    " << namer.getName(b, mod)
           << "[" << attrs(namer, b) << "];\n";
        for (auto ip: b->inputs()) {
            OutputPort* op = conns->findSource(ip);
            os << "    " << namer.getName(op->owner(), mod) << " -> "
               << namer.getName(b, mod) << ";\n";
        }
    }

    os << "}\n";
}

};

