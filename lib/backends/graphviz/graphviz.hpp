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

    void writeModule(FileSet::File* fn, Module* mod,
                     bool trans=true, std::string comment="");
    void writeModule(std::ostream& os, Module* mod,
                     bool trans=true, std::string comment="");
    void write(std::ostream& os, bool trans=true);
};

};

#endif // __LLPM_BACKENDS_GRAPHVIZ_GRAPHVIZ_HPP__
