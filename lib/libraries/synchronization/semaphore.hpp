#ifndef __LLPM_LIBRARIES_SYNCHRONIZATION_SEMAPHORE_HPP__
#define __LLPM_LIBRARIES_SYNCHRONIZATION_SEMAPHORE_HPP__

#include <llpm/design.hpp>
#include <llpm/block.hpp>
#include <libraries/core/interface.hpp>

namespace llpm {

class Semaphore : public Block {
    // Wait request interface
    Interface _wait;
    
    // Port to signal next wait request can proceed
    InputPort _signal;

public:
    Semaphore(Design& d);
    virtual ~Semaphore() {}

    DEF_GET(wait);
    DEF_GET(signal);

    virtual bool hasState() const {
        return true;
    }

    virtual bool refine(ConnectionDB&) const;
    virtual bool refinable() const {
        return true;
    }

    virtual DependenceRule depRule(const OutputPort* op) const {
        assert(op == _wait.dout());
        return DependenceRule(DependenceRule::Custom,
                              DependenceRule::Maybe);
    }

    virtual const std::vector<InputPort*>&
            deps(const OutputPort* op) const {
        assert(op == _wait.dout());
        return inputs();
    }
};

} // namespace llpm

#endif // __LLPM_LIBRARIES_SYNCHRONIZATION_SEMAPHORE_HPP__
