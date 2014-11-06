#ifndef __LLPM_BLOCK_HPP__
#define __LLPM_BLOCK_HPP__

#include <llpm/llpm.hpp>
#include <llpm/history.hpp>
#include <vector>
#include <map>
#include <set>
#include <llpm/ports.hpp>
#include <llpm/exceptions.hpp>
#include <llpm/connection.hpp>
#include <util/macros.hpp>
#include <boost/foreach.hpp>

using namespace std;

namespace llpm {


// Forward decl. Stupid c++!
class Function;
class Module;


/*******
 * Block is the basic unit in LLPM.
 * It can do computation, store state, read inputs, and write
 * outputs. The granularity of a block is not defined -- a block can
 * represent any granularity from a full design to a single gate.
 *
 * Can sometimes be refined into smaller, more granular functions
 * via "refine" method.
 */
class Block : public gc_cleanup {
public:
    enum FiringRule {
        AND, // All inputs must be available before this block is executed
        OR   // Block fires when any input is available
    };

protected:
    Module* _module;
    std::string _name;
    std::set<InputPort*>  _inputs;
    std::set<OutputPort*> _outputs;

    BlockHistory _history;

    Block(): _module(NULL) { }

    friend class InputPort;
    friend class OutputPort;

    virtual void definePort(InputPort* ip) {
        _inputs.insert(ip);
    }
    virtual void definePort(OutputPort* op) {
        _outputs.insert(op);
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

    virtual FiringRule firing() const {
        return AND;
    }

    /*
     * If 'false' (the default), when this block produces an output an output
     * appears on all output ports. If an output can appear on one output but
     * not another then this method must return 'true'.
     */
    virtual bool outputsIndependent() const {
        return false;
    }

    BlockHistory& history() {
        return _history;
    }

    const std::set<InputPort*>&  inputs()  {
        return _inputs;
    }
    const std::set<OutputPort*>& outputs() {
        return _outputs;
    }

    int inputNum(InputPort* ip) const {
        auto f = _inputs.find(ip);
        if (f == _inputs.end())
            return -1;
        else
            return std::distance(_inputs.begin(), f);
    }

    int outputNum(OutputPort* op) const {
        auto f = _outputs.find(op);
        if (f == _outputs.end())
            return -1;
        else
            return std::distance(_outputs.begin(), f);
    }

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
        _din = InputPort(this, input);
        _dout = OutputPort(this, output);
    }

public:
    virtual bool hasState() const {
        return false;
    }

    DEF_GET(din);
    DEF_GET(dout);

};

} // namespace llpm

#endif // __LLPM_BLOCK_HPP__
