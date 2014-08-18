#ifndef __LLPM_REFINERY_HPP__
#define __LLPM_REFINERY_HPP__

#include <type_traits>
#include <typeindex>
#include <vector>
#include <cassert>
#include <boost/foreach.hpp>

#include <llpm/block.hpp>
#include <refinery/priority_collection.hpp>

namespace llpm {

template<class Crude>
class Refinery {
public:
    class Library;

    class Refiner: public PCHandler<std::type_index> {
        Library* _library;

    protected:
        Refiner(Library* lib = NULL) :
            _library(lib)
        { }

    public:
        virtual ~Refiner() { }

        virtual bool handles(std::type_index) const = 0;
        virtual bool refine(std::vector<Crude*>& newCrude, Crude* c) const = 0;

    };

    class StopCondition {
    public:
        virtual bool stop(const std::vector<Crude*>&) = 0;
    };

private:
    PriorityCollection<std::type_index, Refiner> _refiners;

public:
    Refinery() { }
    ~Refinery() { }

    PriorityCollection<std::type_index, Refiner>& refiners() {
        return _refiners;
    }

    unsigned refine(std::vector<Crude*>& crude, StopCondition* sc = NULL);
};

class BlockDefaultRefiner: public Refinery<Block>::Refiner {
public:
    virtual bool handles(std::type_index) const;
    virtual bool refine(std::vector<Block*>& newCrude, Block* c) const;
};


template<class Crude>
unsigned Refinery<Crude>::refine(std::vector<Crude*>& crude, StopCondition* sc) {
    if (sc && sc->stop(crude))
        return false;

    unsigned passes = 0;
    bool foundRefinement;
    do {
        std::vector<Crude*> newCrude;
        foundRefinement = false;
        BOOST_FOREACH(Crude* c, crude) {
            std::type_index type = typeid(c);
            vector<Refiner*> possible_refiners = _refiners(type);
            bool refined = false;
            BOOST_FOREACH(auto r, possible_refiners) {
                if(r->refine(newCrude, c)) {
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
