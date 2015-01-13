#include "graphviz.hpp"

#include <libraries/synthesis/pipeline.hpp>
#include <util/misc.hpp>
#include <util/llvm_type.hpp>
#include <llpm/control_region.hpp>

#include <boost/format.hpp>

using namespace std;

namespace llpm {

string getName(Block* b) {
    return str(boost::format("p%2%")
                % b->name()
                % b);
}

void GraphvizOutput::writeModule(FileSet::File* fn, Module* mod,
                                 bool transparent, string comment) {
    this->writeModule(fn->openStream(), mod, transparent, comment);
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

static std::string quote(string a) {
    string ret = "\"";
    for (auto c: a) {
        switch (c) {
        case '"':
            ret += "\\\"";
            break;
        default:
            ret += c;
            break;
        }
    }
    return ret + "\"";
}

static std::string label(ObjectNamer& namer, Block* b) {
    return quote(namer.getName(b, b->module()) + "\\n" 
                + str(boost::format(
                        "[ %1% ] %2% | %3% | %4%\\n"
                        "%5%\\n") 
                        % b->print()
                        % b->inputs().size()
                        % b->outputs().size()
                        % b->interfaces().size()
                        % b
                    )
                + cpp_demangle(typeid(*b).name()));
}

static std::string attrs(ObjectNamer& namer, Block* b,
                         std::map<std::string, std::string> o) {
    std::map<std::string, std::string> a;
    if (dynamic_cast<PipelineRegister*>(b)) {
        a["shape"] = "rectangle";
        a["label"] = "\"reg " + namer.getName(b, b->module()) + "\"";
    } else {
        a["shape"] = "component";
        a["label"] = label(namer, b);
    }
    
    o.insert(a.begin(), a.end());
    return attrs(o);
}



static std::string attrs(ObjectNamer& namer, OutputPort* op, InputPort* ip,
                         std::map<std::string, std::string> o) {
    std::map<std::string, std::string> a;
    auto opName = op->name();
    if (opName == "")
        opName = str(boost::format("%1%") % op->owner()->outputNum(op));
    auto ipName = ip->name();
    if (ipName == "")
        ipName = str(boost::format("%1%") % ip->owner()->inputNum(ip));
    a["label"] = quote(opName + "\\n" 
        + ipName + "\\n"
        + typestr(op->type()));
    a["fontsize"] = "8.0";

    o.insert(a.begin(), a.end());
    return attrs(o);
}

bool is_hidden(Block* b, Module* topMod) {
    return dynamic_cast<DummyBlock*>(b) != NULL &&
           b->module() != topMod;
}

void printConns(std::ostream& os,
                ObjectNamer& namer,
                Module* mod,
                const ConnectionDB* conns,
                std::map<std::string, std::string> extra_attr,
                bool transparent) {
    for (const auto& conn: *conns) {
        auto op = conn.source();
        auto ip = conn.sink();

        vector<InputPort*> sinks;

        auto opContainer = dynamic_cast<ContainerModule*>(op->owner());
        if (transparent && opContainer != NULL) {
            auto cdb = opContainer->conns();
            auto internalSink = opContainer->getSink(op);
            op = cdb->findSource(internalSink);
        }

        auto ipContainer = dynamic_cast<ContainerModule*>(ip->owner());
        if (transparent && ipContainer != NULL) {
            auto cdb = ipContainer->conns();
            auto internalSource = ipContainer->getDriver(ip);
            cdb->findSinks(internalSource, sinks);
        } else {
            sinks = {ip};
        }

        for (auto ip: sinks) {
            if (is_hidden(op->owner(), mod) ||
                is_hidden(ip->owner(), mod))
                    continue;

            os << "    " << getName(op->owner()) << " -> "
               << getName(ip->owner()) 
               << "[" << attrs(namer, op, ip, extra_attr) << "];\n";
        }
    }
}

void printIO(std::ostream& os,
             ObjectNamer& namer,
             ContainerModule* cm,
             Module* mod) {
    for (InputPort* ip: cm->inputs()) {
        auto dummy = cm->getDriver(ip)->owner();
        os << "    " << getName(dummy)
           << "[" << attrs(namer, dummy,
                           {{"label", ip->name()},
                            {"shape", "house"}}) << "];\n";
    }

    for (OutputPort* op: cm->outputs()) {
        auto dummy = cm->getSink(op)->owner();
        os << "    " << getName(dummy)
           << "[" << attrs(namer, dummy,
                           {{"label", op->name()},
                            {"shape", "invhouse"}}) << "];\n";
    }
}

void printBlock(std::ostream& os,
                ObjectNamer& namer,
                Module* mod,
                Block* block,
                bool transparent)
{
    ContainerModule* cm = dynamic_cast<ContainerModule*>(block);
    if (transparent && cm != NULL) {
        os << boost::format("    subgraph cluster_%1% {\n"
                            "        color=black;\n"
                            "        label=%2%;\n")
                                % cm->name()
                                % label(namer, cm);

        vector<Block*> crblocks;
        cm->blocks(crblocks);
        for (Block* b: crblocks) {
            printBlock(os, namer, mod, b, transparent);
        }
        printConns(os, namer, mod, cm->conns(),
                   {{"style", "dashed"}}, transparent);
        os << "    }\n";

    } else {
        std::map<std::string, std::string> ea;
        os << "        " << getName(block)
           << "[" << attrs(namer, block, ea) << "];\n";
    }
}

void GraphvizOutput::writeModule(std::ostream& os, Module* mod,
                                 bool transparent, string comment) {
    ObjectNamer& namer = mod->design().namer();

    ConnectionDB* conns = mod->conns();
    assert(conns != NULL);

    os << "digraph " << mod->name() << " {\n";
    os << "    size=100;\n";
    os << "    labelsize=28;\n";
    os << "    label=" << quote(comment) << ";\n";


    ContainerModule* cmMod = dynamic_cast<ContainerModule*>(mod);
    if (cmMod) {
        printIO(os, namer, cmMod, mod);
    }

    vector<Block*> blocks;
    mod->blocks(blocks);

    for (auto block: blocks) {
        printBlock(os, namer, mod, block, transparent);
    }

    printConns(os, namer, mod, conns,
               {{"style", "bold"}}, transparent);

    os << "}\n";
}

};

