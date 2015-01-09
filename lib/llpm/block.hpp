#ifndef __LLPM_BLOCK_HPP__
#define __LLPM_BLOCK_HPP__

#include <llpm/llpm.hpp>
#include <llpm/history.hpp>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <llpm/ports.hpp>
#include <llpm/exceptions.hpp>
#include <llpm/connection.hpp>
#include <util/macros.hpp>
#include <boost/foreach.hpp>

namespace llpm {

// Forward decl. Stupid c++!
class Function;
class Module;
class Interface;

/*******
 * Block is the basic unit in LLPM.
 * It can do computation, store state, read inputs, and write
 * outputs. The granularity of a block is not defined -- a block can
 * represent any granularity from a full design to a single gate.
 *
 * Can sometimes be refined into smaller, more granular functions
 * via "refine" method.
 */
class Block {
protected:
    Module* _module;
    std::string _name;
    std::vector<InputPort*>  _inputs;
    std::vector<OutputPort*> _outputs;
    std::vector<Interface*>  _interfaces;

    BlockHistory _history;

    Block(): _module(NULL) { }

    friend class InputPort;
    friend class OutputPort;
    friend class Interface;

    virtual void definePort(InputPort* ip) {
        assert(ip->owner() == this);
        // assert(((uint64_t)(void*)ip) < 0x700000000000);
        if (std::find(_inputs.begin(), _inputs.end(), ip) == _inputs.end())
            _inputs.push_back(ip);
    }
    virtual void definePort(OutputPort* op) {
        assert(op->owner() == this);
        // assert(((uint64_t)(void*)op) < 0x700000000000);
        if (std::find(_outputs.begin(), _outputs.end(), op) == _outputs.end())
            _outputs.push_back(op);
    }
    virtual void defineInterface(Interface* iface) {
        if (std::find(_interfaces.begin(), _interfaces.end(), iface) == 
                _interfaces.end())
            _interfaces.push_back(iface);
    }


public:
    virtual ~Block() { }

    Module* module() const {
        return _module;
    }
    void module(Module* m) {
        if (m == NULL)
            throw InvalidArgument("Module cannot be NULL!");
        _module = m;
    }

    DEF_GET_NP(name);
    DEF_SET(name);

    // Upon what conditions does a block accept inputs and execute?
    // This routine only works properly when all the outputs have the same
    // dependence rules. If not, it returns "Custom"
    DependenceRule::InputType firing() const;

    // False if this block has multiple outputs and their dependences
    // differ or are maybes. Otherwise, output tokens are always produced
    // based on the same rules, so they are "dependent"
    bool outputsIndependent() const;

    // Find the dependence rule for an output port
    virtual DependenceRule depRule(const OutputPort*) const = 0;
    // Find all of the inputs upon which an output depends.
    virtual const std::vector<InputPort*>& deps(const OutputPort*) const = 0;

    // Can this output be pipelined? Almost always yes, but in some cases
    // doing so may create distributed state, which must be specially dealt
    // with. For now, we punt and disallow pipelining in those cases.
    virtual bool pipelineable(const OutputPort*) const {
        return true;
    }

    // Does the logic in this block contain any cycles? Cycles mean that
    // logic cannot be completely combinatorial and prevents static
    // region formation
    virtual bool hasCycle() const {
        return false;
    }

     BlockHistory& history() {
        return _history;
    }

    const std::vector<InputPort*>&  inputs() const {
        return _inputs;
    }
    const std::vector<OutputPort*>& outputs() const {
        return _outputs;
    }
    const std::vector<Interface*>& interfaces() const {
        return _interfaces;
    }

    void ports(std::vector<InputPort*>& iports) const {
        iports.insert(iports.end(), _inputs.begin(), _inputs.end());
    }
    void ports(std::vector<OutputPort*>& oports) const {
        oports.insert(oports.end(), _outputs.begin(), _outputs.end());
    }

    // Can previous blocks re-order the fields of this input and obtain the
    // same result from this block?
    virtual bool inputCommutative(InputPort* op) const {
        return false;
    }

    int inputNum(InputPort* ip) const {
        auto f = find(_inputs.begin(), _inputs.end(), ip);
        if (f == _inputs.end())
            return -1;
        else
            return std::distance(_inputs.begin(), f);
    }

    int outputNum(OutputPort* op) const {
        auto f = find(_outputs.begin(), _outputs.end(), op);
        if (f == _outputs.end())
            return -1;
        else
            return std::distance(_outputs.begin(), f);
    }

    // Does this block contain architectural state? Answering false does
    // not preclude microarchitectural state (like pipeline registers)
    // from being added.
    virtual bool hasState() const = 0;
    bool isPure() const {
        return !hasState();
    }
    
    virtual bool refinable() const {
        return false;
    }

    /* Refines a block into smaller blocks. Usually a default
     * refinement -- libraries may override it.
     *
     * @return Returns false if this block does not know how to
     * refine itself. A library may be able to refine it further.
     *
     * @arg blocks Output blocks stored here
     * @arg ipMap Mapping of input ports in original block to input
     *            ports in new blocks
     * @arg opMap Mapping of output ports in original block to
     *            output ports in new blocks
     */
    virtual bool refine(ConnectionDB& conns) const {
        if (refinable())
            throw ImplementationError("Block needs to implement refine method!");
        return false;
    }

    virtual std::string print() const {
        return "";
    }
};

/*****
 * A purely functional block.
 * Takes one input and produces one output. No state
 */
class Function : public virtual Block {
protected:
    InputPort _din;
    OutputPort _dout;

    Function(llvm::Type* input, llvm::Type* output) :
        _din(this, input, "x"),
        _dout(this, output, "a")
    { }

    virtual ~Function() { }

    void resetTypes(llvm::Type* input, llvm::Type* output) {
        _din.reset(input);
        _dout.reset(output);
    }

public:
    virtual bool hasState() const {
        return false;
    }

    DEF_GET(din);
    DEF_GET(dout);

    Join* injoin(ConnectionDB& conns) {
        return _din.join(conns);
    }
    InputPort* din(ConnectionDB& conns, unsigned idx) {
        return _din.join(conns, idx);
    }
    Split* outsplit(ConnectionDB& conns) {
        return _dout.split(conns);
    }
    OutputPort* dout(ConnectionDB& conns, unsigned idx) {
        return _dout.split(conns, idx);
    }

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND,
                              DependenceRule::Always);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(op == &_dout);
        return inputs();
    }
};

} // namespace llpm

#endif // __LLPM_BLOCK_HPP__
