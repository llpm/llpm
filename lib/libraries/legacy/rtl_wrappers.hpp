#ifndef __LLPM_LIBRARIES_LEGACY_RTL_WRAPPERS_HPP__
#define __LLPM_LIBRARIES_LEGACY_RTL_WRAPPERS_HPP__

#include <llpm/block.hpp>
#include <llpm/module.hpp>
#include <util/llvm_type.hpp>

#include <map>
#include <vector>

namespace llpm {

class Pin {
    std::string _name;
public:
    Pin(std::string name) :
        _name(name)
    { }

    Pin() { }

    DEF_GET_NP(name);
    DEF_SET(name);
};

/**
 * All of the different methods to map an LLPM channel into RTL must
 * extend this class
 */
class RTLTranslator {
};

/**
 * When an LLPM channel has to connect to an RTL bus which is
 * semantically LI, this translator specifies how to map the signals.
 */
class LIBus : public RTLTranslator {
public:
    enum class BPStyle {
        Missing, // There is no backpressure
        Wait,    // Signal when port is backpressuring
        Ready    // Signal when port can receive
    };

private:
    Port* _port;
    BPStyle _bpStyle;;
    Pin _valid;
    Pin _bp;
    std::vector<Pin> _pins;

public:
    LIBus(Port* p, BPStyle style) :
        _port(p),
        _bpStyle(style)
    {
        if (bitwidth(p->type()) > 0) {
            auto nct = numContainedTypes(p->type());
            if (nct >= 1)
                _pins.resize(nct);
            else
                _pins.resize(1);
        }
    }

    LIBus(Port* p, BPStyle style, const std::vector<std::string>& names) :
        _port(p),
        _bpStyle(style),
        _valid(names[0]),
        _bp(names[1])
    {
        if (bitwidth(p->type()) > 0) {
            auto nct = numContainedTypes(p->type());
            if (nct >= 1)
                _pins.resize(nct);
            else
                _pins.resize(1);

            if (names.size() != _pins.size() + 2)
                throw InvalidArgument("Must have equal number of names as pins!");

            for (unsigned i=0; i<_pins.size(); i++) {
                _pins[i].name(names[i+2]);
            }
        }
    }

    Pin& pin(unsigned pi) {
        assert(pi < _pins.size());
        return _pins[pi];
    }

};

/**
 * This class wraps an LLPM Module into an RTL module shell. In order
 * to work properly, the backend has to know about this module, since
 * it's special.
 */
class WrapLLPMMModule : public Module {
    Module* _wrapped;
    std::map<Port*, RTLTranslator*> _pinDefs;

public:
    WrapLLPMMModule(Module* mod) :
        Module(mod->design(), mod->name() + "rtlwrapper"),
        _wrapped(mod)
    { }

    DEF_GET_NP(wrapped);

    void specifyTranslator(Port* p, RTLTranslator* trans) {
        _pinDefs[p] = trans;
    }

    const std::map<Port*, RTLTranslator*>& pinDefs() const {
        return _pinDefs;
    }

    virtual bool hasCycle() const {
        return _wrapped->hasCycle();
    }
    virtual bool hasState() const {
        return _wrapped->hasState();
    }
    virtual void blocks(std::vector<Block*>& vec) const {
        vec.push_back(_wrapped);
    }
    virtual void submodules(std::vector<Module*>& vec) const {
        vec.push_back(_wrapped);
    }
    virtual void submodules(std::vector<Block*>& vec) const {
        vec.push_back(_wrapped);
    }
    virtual unsigned internalRefine(
        int depth = -1,
        Refinery::StopCondition* sc = NULL) {
        if (depth > 0)
            depth--;
        if (depth == 0)
            return 0;
        return _wrapped->internalRefine(depth, sc);
    }
    virtual uint64_t changeCounter() {
        return _wrapped->changeCounter();
    }

    virtual void validityCheck() const;

    virtual DependenceRule depRule(const OutputPort*) const {
        // This module has no ports!
        assert(false);
    }
    virtual const std::vector<InputPort*>&
        deps(const OutputPort*) const {
        // This module has no ports!
        assert(false);
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_LEGACY_RTL_WRAPPERS_HPP__
