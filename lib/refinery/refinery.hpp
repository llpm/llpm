#ifndef __LLPM_REFINERY_HPP__
#define __LLPM_REFINERY_HPP__

#include <type_traits>
#include <typeindex>
#include <vector>
#include <boost/foreach.hpp>

#include <refinery/priority_collection.hpp>

namespace llpm {

template<class Crude>
class Refinery {
public:
    class Refiner: public PCHandler<std::type_index>{
    public:
        Refiner() { }
        virtual ~Refiner() { }

        virtual bool handles(std::type_index) const = 0;
        virtual void refine(std::vector<Crude*>& newCrude, Crude* c) const = 0;
    };

private:
    PriorityCollection<std::type_index, Refiner> _refiners;

public:
    Refinery() { }
    ~Refinery() { }

    PriorityCollection<Crude, Refiner>& refiners() {
        return _refiners;
    }

    void refine(std::vector<Crude*> crude, bool exhaust = true);
};


template<class Crude>
void Refinery<Crude>::refine(std::vector<Crude*> crude, bool exhaust) {
    bool foundRefinement;
    do {
        std::vector<Crude*> newCrude;
        foundRefinement = false;
        BOOST_FOREACH(Crude* c, crude) {
            std::type_index type = typeid(c);
            Refiner* r = _refiners(type);
            if (r != NULL) {
                foundRefinement = true;
                r->refine(newCrude, c);
            }
        }
        crude.swap(newCrude);
    } while (exhaust && foundRefinement);
}

} // namespace llpm

#endif // __LLPM_REFINERY_HPP__
