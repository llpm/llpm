#ifndef __LLPM_WEDGES_WEDGE_HPP__
#define __LLPM_WEDGES_WEDGE_HPP__

#include <util/files.hpp>

namespace llpm {

// Fwd defs
class Module;

class Wedge {
public:
    virtual ~Wedge() { }

    virtual void writeModule(FileSet& fileset, Module* mod) = 0;
};

} // llpm

#endif // __LLPM_WEDGES_WEDGE_HPP__
