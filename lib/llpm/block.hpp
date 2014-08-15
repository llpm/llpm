#ifndef __LLPM_BLOCK_HPP__
#define __LLPM_BLOCK_HPP__

#include <vector>
#include <map>
#include <llpm/ports.hpp>
#include <llpm/exceptions.hpp>
#include <util/macros.hpp>

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
class Block {
protected:
    Module* _module;
    vector<InputPort*>  _inputs;
    vector<OutputPort*> _outputs;

    Block(): _module(NULL) { }
    virtual ~Block() { }

public:
    Module* module() const {
        return _module;
    }
    void module(Module* m) {
        if (m == NULL)
            throw InvalidArgument("Module cannot be NULL!");
        _module = m;
    }

    vector<InputPort*>&  inputs()  {
        return _inputs;
    }
    vector<OutputPort*>& outputs() {
        return _outputs;
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
    virtual bool refine(std::vector<Block*>& blocks,
                        std::map<InputPort*, vector<InputPort*> >& ipMap,
                        std::map<OutputPort*, OutputPort*>& opMap) const {
        if (refinable())
            throw ImplementationError("Block needs to implement refine method!");
        return false;
    }


    /* Refines this block just like the other refine method, but
     * modifies connections referring to the input and output ports
     * based on the mappings returned in the other refine method.
     */
    virtual bool refine(std::vector<Block*>& blocks);
};

/*****
 * A purely functional block.
 * Takes one input and produces one output. No state
 */
class Function : public Block {
protected:
    InputPort _din;
    OutputPort _dout;

    Function(llvm::Type* input, llvm::Type* output) :
        _din(this, input),
        _dout(this, output) {

        _inputs = {&_din};
        _outputs = {&_dout};
    }

    virtual ~Function() { }

public:
    virtual bool hasState() const {
        return false;
    }

    DEF_GET(din);
    DEF_GET(dout);

};

} // namespace llpm

#endif // __LLPM_BLOCK_HPP__
