#ifndef __LLPM_BLOCK_HPP__
#define __LLPM_BLOCK_HPP__

#include <llpm/llpm.hpp>
#include <llpm/history.hpp>
#include <vector>
#include <llpm/ports.hpp>
#include <llpm/exceptions.hpp>
#include <util/macros.hpp>

#include <map>
#include <set>
#include <algorithm>

#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>


namespace llpm {

// Forward decl. Stupid c++!
class Function;
class Module;
class Interface;
class ConnectionDB;

class Block;
typedef boost::intrusive_ptr<Block> BlockP;

/**
 * Block is the basic unit in LLPM.
 * It can do computation, store state, read inputs, and write
 * outputs. The granularity of a block is not defined -- a block can
 * represent any granularity from a full design to a single gate.
 *
 * Can sometimes be refined into smaller, more granular functions
 * via "refine" method.
 */
class Block :
    public boost::intrusive_ref_counter<Block,
                                        boost::thread_unsafe_counter>
{
protected:
    Module* _module;
    std::string _name;
    std::vector<InputPort*>  _inputs;
    std::vector<OutputPort*> _outputs;
    std::vector<Interface*>  _interfaces;

    BlockHistory _history;

    Block(): _module(nullptr) { }

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

    BlockP getptr() {
        return boost::intrusive_ptr<Block>(this);
    }

    template<typename T, typename ... Args>
    static std::shared_ptr<T> Create(Args ... args) {
        return std::shared_ptr<T>(new T(args...));
    }

    Module* module() const {
        return _module;
    }
    void module(Module* m) {
        _module = m;
    }

    DEF_GET_NP(name);
    DEF_SET(name);

    template<typename TEST>
    bool is() const {
        return dynamic_cast<const TEST*>(this) != NULL;
    }

    template<typename TEST>
    bool isnot() const {
        return dynamic_cast<const TEST*>(this) == NULL;
    }

    template<typename TO>
    TO* as() {
        return dynamic_cast<TO*>(this);
    }

    std::string globalName() const;

    /**
     * Upon what conditions does a block accept inputs and execute?
     * This routine only works properly when all the outputs have the
     * same dependence rules. If not, it returns "Custom"
     */
    DependenceRule::DepType firing() const;

    /**
     * When one output fires, do all of them them? If so, this method
     * should answer yes.
     */
    virtual bool outputsTied() const;

    /**
     * True iff one of the two conditions holds:
     *    - A single block firing results in one output token on one
     *    output port
     *    - The output ports are buffered and deal with backpressure
     *    independently from each other.
     */
    virtual bool outputsSeparate() const {
        // Nearly nobody does this, so default to no
        return false;
    }

    /**
     * Find the dependence rule for an output port 
     */
    virtual DependenceRule deps(const OutputPort* op) const {
        return DependenceRule(DependenceRule::Custom,
                              inputs());
    }

    /**
     * Calls the above and inserts into a set
     */
    virtual void deps(const OutputPort*, std::set<const InputPort*>&) const;
    /**
     * Based on the above deps function, find all of the output ports
     * which an input may affect 
     */
    virtual void deps(const InputPort*, std::set<const OutputPort*>&) const;

    /**
     * What is the logical effort expended in getting a token from
     * this block's InputPort i to OutputPort o?
     *
     * Logical effort is intended to be some abstract measure
     * correlated to the amount of computation involved. Maybe there
     * is some good way to formalize this (like the Sutherland's
     * logical effort in VLSI design), but we're not doing that yet.
     */
    virtual float logicalEffort(const InputPort*, const OutputPort*) const {
        return 0.0;
    }

    float maxLogicalEffort(OutputPort*) const;

    /**
     * Does the logic in this block contain any cycles? Cycles mean
     * that logic cannot be completely combinatorial and prevents
     * static region formation
     */
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

    /**
     * Can previous blocks re-order the fields of this input and
     * obtain the / same result from this block?
     */
    virtual bool inputCommutative(InputPort*) const {
        return false;
    }

    int inputNum(const InputPort* ip) const {
        auto f = find(_inputs.begin(), _inputs.end(), ip);
        if (f == _inputs.end())
            return -1;
        else
            return std::distance(_inputs.begin(), f);
    }

    int outputNum(const OutputPort* op) const {
        auto f = find(_outputs.begin(), _outputs.end(), op);
        if (f == _outputs.end())
            return -1;
        else
            return std::distance(_outputs.begin(), f);
    }

    /**
     * Does this block contain architectural state? Answering false
     * does not preclude microarchitectural state (like pipeline
     * registers) from being added. 
    */
    virtual bool hasState() const = 0;
    bool isPure() const {
        return !hasState();
    }
    
    virtual bool refinable() const {
        return false;
    }

    /**
     * Refines a block into smaller blocks. Usually a default
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
    virtual bool refine(ConnectionDB&) const {
        if (refinable())
            throw ImplementationError(
                "Block needs to implement refine method!");
        return false;
    }

    virtual std::string print() const {
        return "";
    }
};

/**
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

    virtual float logicalEffortFunc() const {
        return 0.0;
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


    float logicalEffort(const InputPort*, const OutputPort*) const {
        return logicalEffortFunc();
    }

    virtual DependenceRule deps(const OutputPort* op) const {
        assert(op == &_dout);
        return DependenceRule(DependenceRule::AND_FireOne,
                              {&_din});
    }
};

} // namespace llpm

#endif // __LLPM_BLOCK_HPP__
