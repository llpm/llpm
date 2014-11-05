#ifndef __LLPM_BACKENDS_GRAPHVIZ_GRAPHVIZ_HPP__
#define __LLPM_BACKENDS_GRAPHVIZ_GRAPHVIZ_HPP__

#include <llpm/block.hpp>
#include <llpm/module.hpp>
#include <synthesis/object_namer.hpp>
#include <util/files.hpp>

namespace llpm {

class GraphvizOutput {
    Design& _design;

public:
    GraphvizOutput(Design& design) :
        _design(design)
    { }

    void writeModule(FileSet::File* fn, Module* mod);
    void writeModule(std::ostream& os, Module* mod);
    void write(std::ostream& os);
};

};

#endif // __LLPM_BACKENDS_GRAPHVIZ_GRAPHVIZ_HPP__
