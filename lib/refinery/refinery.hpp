#ifndef __LLPM_REFINERY_HPP__
#define __LLPM_REFINERY_HPP__

#include <type_traits>
#include <typeindex>
#include <vector>
#include <cassert>
#include <boost/function.hpp>
#include <unordered_set>

#include <llpm/block.hpp>
#include <llpm/connection.hpp>
#include <refinery/priority_collection.hpp>

namespace llpm {

class Refinery {
public:
    class Refiner;
    typedef llpm::PriorityCollection<Block, Refiner> PCollection;
    typedef llpm::Library<Refiner> Library;

    class Refiner: public PCHandler<Block> {
        // Library* _library;

    protected:
        Refiner(Library* lib = NULL)
            // _library(lib)
        { }

    public:
        virtual ~Refiner() { }

        virtual bool handles(Block*) const = 0;
        virtual bool refine(const Block* c,
                            ConnectionDB& conns) const = 0;

    };

    class StopCondition {
    public:
        virtual bool stopRefine(Block*) = 0;
        virtual void unrefined(std::vector<Block*>& crude) {
            std::vector<Block*> unref;
            for(Block* c: crude) {
                if (!stopRefine(c))
                    unref.push_back(c);
            }
            unref.swap(crude);
        }
        virtual bool refined(const std::vector<Block*>& crude) {
            for (Block* c: crude) {
                if (this->stopRefine(c) == false)
                    return false;
            }
            return true;
        }
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

    unsigned refine(std::vector<Block*> crude,
                    ConnectionDB& conns,
                    int depth = -1,
                    StopCondition* sc = NULL);
};


class BaseLibraryStopCondition: public Refinery::StopCondition
{
    std::unordered_map<std::type_index, bool> _classes;
    std::map<std::type_index, boost::function<bool (Block* c)> > _tests;

    template<typename C>
    static bool Test(Block* c) {
        return dynamic_cast<C*>(c) != NULL;
    }

public:
    BaseLibraryStopCondition() { }
    virtual ~BaseLibraryStopCondition() { }

    virtual bool stopRefine(Block* c) {
        std::type_index idx = typeid(*c);
        auto f = _classes.find(idx);
        if (f != _classes.end())
            return f->second;

        for(auto&& test: _tests) {
            if (test.second(c)) {
                _classes[idx] = true;
                return true;
            }
        }

        _classes[idx] = false;
        return false;
    }

    template<typename C>
    void addClass() {
        // Invalidate the cache
        _classes.clear();

        // Add the test
        _tests[typeid(C)] = Test<C>;
    }
};


/* Allows a refining class to define a non-modifying refine method.
 * Implements the modifying version based on it.
 */
class BlockRefiner: public Refinery::Refiner {
public:
    virtual bool refine(
        const Block* block,
        ConnectionDB& conns) const = 0;
    virtual bool refine(Block* c) const;
};

class BlockDefaultRefiner: public BlockRefiner {
public:
    virtual bool handles(Block*) const;
    virtual bool refine(
        const Block* block,
        ConnectionDB& conns) const;
};

} // namespace llpm

#endif // __LLPM_REFINERY_HPP__
