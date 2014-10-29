#include "object_namer.hpp"

#include <llpm/block.hpp>
#include <llpm/ports.hpp>
#include <llpm/module.hpp>

#include <boost/format.hpp>

namespace llpm {

static std::string addContext(std::string base, Block* b, Module* ctxt) {
    assert(ctxt != NULL);
    std::string name = base;
    Module* mod = b->module();
    while (mod != ctxt && mod != NULL) {
        name = mod->name() + "_" + name;
        mod = mod->module();
        if (mod == NULL) {
            throw InvalidArgument("Module ctxt does not appear to be a parent of Block b!");
        }
    }

    return name;
}

std::string ObjectNamer::primBlockName(Block* b) {
    std::string& base = _blockNames[b];
    if (base == "") {
        base = b->name();
        if (base == "")
            base = str(boost::format("anonBlock%1%") % ++anonBlockCounter);

        size_t ctr = 0;
        string orig_base = base;
        while (_existingNames.count(std::make_pair(b->module(), base))) {
            base = str(boost::format("%1%_%2%") % orig_base % ++ctr);
        }
    }
    return base;
}

std::string ObjectNamer::getName(Block* b, Module* ctxt, bool ignore) {
    std::string base = primBlockName(b);
    return addContext(base, b, ctxt);
}

std::string ObjectNamer::getName(InputPort* p, Module* ctxt, bool io) {
    std::string& base = _portNames[p];
    if (base == "") {
        base = p->name();
        if (base == "") {
            unsigned count = 0;
            for (auto&& ip: p->owner()->inputs()) {
                if (ip == p)
                    break;
                count++;
            }
            assert(count < p->owner()->inputs().size());
            base = str(boost::format("%1%") % count);
        }

        if (!io)
            base = str(boost::format("%1%_input%2%") % primBlockName(p->owner()) % base);
    }
    return addContext(base, p->owner(), ctxt);
}

std::string ObjectNamer::getName(OutputPort* p, Module* ctxt, bool io) {
    std::string& base = _portNames[p];
    if (base == "") {
        base = p->name();
        if (base == "") {
            unsigned count = 0;
            for (auto&& op: p->owner()->outputs()) {
                if (op == p)
                    break;
                count++;
            }
            assert(count < p->owner()->outputs().size());
            base = str(boost::format("%1%") % count);
        }

        if (!io)
            base = str(boost::format("%1%_output%2%") % primBlockName(p->owner()) % base);
    }
    return addContext(base, p->owner(), ctxt);
}

}
