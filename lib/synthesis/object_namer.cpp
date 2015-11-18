#include "object_namer.hpp"

#include <llpm/block.hpp>
#include <llpm/ports.hpp>
#include <llpm/module.hpp>

#include <boost/format.hpp>
#include <cctype>

using namespace std;

namespace llpm {

static std::string addContext(std::string base, Block* b, Module* ctxt) {
    if (ctxt == nullptr)
        return base;
    std::string name = base;
    Module* mod = b->module();
    while (b != ctxt && mod != ctxt && mod != NULL) {
        name = mod->name() + "_" + name;
        mod = mod->module();
        if (mod == NULL) {
            throw InvalidArgument("Module ctxt does not appear to be a parent of Block b!");
        }
    }

    return name;
}

void ObjectNamer::assignName(Port* p, Module* ctxt, std::string name) {
    _portNames[make_pair(ctxt, p)] = name;
    _existingNames.insert(make_pair(ctxt, name));
}

static std::string sanitize(std::string s) {
    for (unsigned i = 0; i < s.size(); i++) {
        if (!isalnum(s[i])) {
            s[i] = '_';
        }
    }
    return s;
}

std::string ObjectNamer::historicalName(Block* b) {
    if (b->name() != "")
        return b->name();
    if (b->history().src() == BlockHistory::Refinement &&
        b->history().hasSrcBlock()) {
        return historicalName(b->history().srcBlock()) + "r";
    } else if (b->history().src() == BlockHistory::Optimization &&
               b->history().hasSrcBlock()) {
        return historicalName(b->history().srcBlock()) + "o";
    }

    return str(boost::format("anonBlock%1%") % ++anonBlockCounter);
}

std::string ObjectNamer::primBlockName(Block* b, Module* ctxt) {
    std::string& base = _blockNames[make_pair(ctxt, b)];
    if (base == "") {
        base = b->name();
        if (base == "") {
            base = historicalName(b);
        } else {
            base = sanitize(base);
        }

        size_t ctr = 0;
        string orig_base = base;
        while (_existingNames.count(std::make_pair(ctxt, base)) > 0 ||
               _existingNames.count(std::make_pair(nullptr, base)) > 0) {
            base = str(boost::format("%1%_%2%") % orig_base % ++ctr);
        }
        _existingNames.insert(make_pair(ctxt, base));
    }
    return base;
}

std::string ObjectNamer::getName(Block* b, Module* ctxt, bool) {
    std::string base = primBlockName(b, ctxt);
    return addContext(base, b, ctxt);
}

std::string ObjectNamer::getName(InputPort* p, Module* ctxt, bool io) {
    if (io)
        return p->name();

    std::string& base = _portNames[make_pair(ctxt, p)];
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
            base = str(boost::format("%1%_input%2%") 
                        % primBlockName(p->owner(), ctxt)
                        % base);
    }
    return addContext(base, p->owner(), ctxt);
}

std::string ObjectNamer::getName(OutputPort* p, Module* ctxt, bool io) {
    if (io)
        return p->name();

    std::string& base = _portNames[make_pair(ctxt, p)];
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
            base = str(boost::format("%1%_output%2%")
                       % primBlockName(p->owner(), ctxt)
                       % base);
    }
    return addContext(base, p->owner(), ctxt);
}

}
