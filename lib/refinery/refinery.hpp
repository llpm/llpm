#ifndef __LLPM_REFINERY_HPP__
#define __LLPM_REFINERY_HPP__

#include <type_traits>
#include <typeindex>
#include <vector>
#include <cassert>
#include <boost/foreach.hpp>

#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <refinery/priority_collection.hpp>

namespace llpm {

template<class Crude>
class Refinery {
public:
    class Refiner;
    typedef llpm::PriorityCollection<Block, Refiner> PCollection;
    typedef llpm::Library<Refiner> Library;

    class Refiner: public PCHandler<Block> {
        Library* _library;

    protected:
        Refiner(Library* lib = NULL) :
            _library(lib)
        { }

    public:
        virtual ~Refiner() { }

        virtual bool handles(Block*) const = 0;
        virtual bool refine(const Crude* c,
                            std::vector<Crude*>& newCrude,
                            ConnectionDB& conns) const = 0;

    };

    class StopCondition {
    public:
        virtual bool stop(const std::vector<Crude*>&) = 0;
    };

private:
    PCollection _refiners;

public:
    Refinery() { }
    ~Refinery() { }

    PCollection& refiners() {
        return _refiners;
    }

    void appendLibrary(Library* lib) {
        refiners().appendLibrary(lib);
    }

    void prependLibrary(Library* lib) {
        refiners().prependLibrary(lib);
    }

    unsigned refine(std::vector<Crude*>& crude,
                    ConnectionDB& conns,
                    StopCondition* sc = NULL);
};


/* Allows a refining class to define a non-modifying refine method.
 * Implements the modifying version based on it.
 */
class BlockRefiner: public Refinery<Block>::Refiner {
public:
    virtual bool refine(
        const Block* block,
        std::vector<Block*>& blocks,
        ConnectionDB& conns) const = 0;
    virtual bool refine(Block* c) const;
};

class BlockDefaultRefiner: public BlockRefiner {
public:
    virtual bool handles(Block*) const;
    virtual bool refine(
        const Block* block,
        std::vector<Block*>& blocks,
        ConnectionDB& conns) const;
};


template<class Crude>
unsigned Refinery<Crude>::refine(std::vector<Crude*>& crude,
                                 ConnectionDB& conns,
                                 StopCondition* sc) {
    if (sc && sc->stop(crude))
        return false;

    unsigned passes = 0;
    bool foundRefinement;
    do {
        std::vector<Crude*> newCrude;
        foundRefinement = false;
        BOOST_FOREACH(Crude* c, crude) {
            const vector<Refiner*>& possible_refiners = _refiners(c);
            bool refined = false;
            BOOST_FOREACH(auto r, possible_refiners) {
                if(r->refine(c, newCrude, conns)) {
                    refined = true;
                    break;
                }
            }
            if (!refined) {
                newCrude.push_back(c);
            } else {
                foundRefinement = true;
            }
        }
        crude.swap(newCrude);
        if (foundRefinement)
            passes += 1;
    } while (foundRefinement && sc && !sc->stop(crude));

    return passes;
}

} // namespace llpm

#endif // __LLPM_REFINERY_HPP__
