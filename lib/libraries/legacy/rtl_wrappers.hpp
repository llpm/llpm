#pragma once

#include <llpm/block.hpp>
#include <llpm/module.hpp>
#include <util/llvm_type.hpp>

#include <map>
#include <vector>

namespace llpm {

//fwd defs
class IPXactBackend;

class Pin {
    std::string _name;
    bool _input;
    unsigned _width;
public:
    Pin(std::string name) :
        _name(name),
        _width(1)
    { }

    Pin() :
        _width(1)
    { }

    DEF_GET_NP(name);
    DEF_SET(name);
    DEF_GET_NP(input);
    DEF_SET(input);
    DEF_GET_NP(width);
    DEF_SET(width);
};

/**
 * All of the different methods to map an LLPM channel into RTL must
 * extend this class
 */
class RTLTranslator {
protected:
    virtual void dummy() {} 

    std::vector<Pin> _pins;

public:
    Pin& pin(unsigned pi) {
        assert(pi < _pins.size());
        return _pins[pi];
    }

    DEF_ARRAY_GET(pins);
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

public:
#if 0
    LIBus(Port* p, BPStyle style) :
        _port(p),
        _bpStyle(style)
    {
        if (bitwidth(p->type()) > 0) {
            auto nct = numContainedTypes(p->type());
            if (nct >= 1)
                _pins.resize(nct+2);
            else
                _pins.resize(3);
        }
    }
#endif

    LIBus(Port* p, BPStyle style, const std::vector<std::string>& names) :
        _port(p),
        _bpStyle(style)
    {
        auto nct = numContainedTypes(p->type());
        if (nct >= 1)
            _pins.resize(nct+2);
        else
            _pins.resize(3);

        if (names.size() != _pins.size())
            throw InvalidArgument("Must have equal number of names as pins!");

        bool input = p->asInput() != nullptr;
        for (unsigned i=0; i<_pins.size(); i++) {
            _pins[i].name(names[i]);
            _pins[i].input(input);

            if (i >= 2) {
                if (nct >= 1)
                    _pins[i].width( bitwidth(nthType(p->type(), i-2)) );
                else
                    _pins[i].width( bitwidth(p->type()) );
            }
        }
        _pins[1].input(!input);
    }

    DEF_GET_NP(port);
    DEF_GET_NP(bpStyle);

    Pin valid() const {
        return _pins[0];
    }
    Pin bp() const {
        return _pins[1];
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
        Module(mod->design(), mod->name() + "_rtl"),
        _wrapped(mod)
    { }

    DEF_GET_NP(wrapped);

    void specifyTranslator(Port* p, RTLTranslator* trans) {
        _pinDefs[p] = trans;
    }

    const std::map<Port*, RTLTranslator*>& pinDefs() const {
        return _pinDefs;
    }

    void writeMetadata(IPXactBackend*) const;

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
    virtual uint64_t changeCounter() const {
        return _wrapped->changeCounter();
    }

    virtual void validityCheck() const;

    virtual DependenceRule deps(const OutputPort*) const {
        // This module has no ports!
        assert(false);
    }
};

} // namespace llpm

