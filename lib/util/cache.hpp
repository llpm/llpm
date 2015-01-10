#ifndef __LLPM_UTIL_CACHE_HPP__
#define __LLPM_UTIL_CACHE_HPP__

#include <stdint.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>

namespace llpm {

template<typename T>
class Cache {
    bool initialized;
    T data;
    uint64_t dataAge;
    const uint64_t* counter;
    boost::function<T(void)> update;

public:
    Cache() : initialized(false) {
    }

    Cache(const uint64_t* counter,
          boost::function<T(void)> update) :
        initialized(true),
        dataAge((*counter)-1),
        counter(counter),
        update(update)
    { }

    void setup(const uint64_t* counter,
               boost::function<T(void)> update) {
        this->counter = counter;
        this->update = update;
        this->dataAge = (*counter) - 1;
        this->initialized = true;
   }

    void reset() {
        dataAge = (*counter) - 1;
    }
    
    T get() {
        assert(initialized);
        if (*counter != dataAge) {
            data = update();
            dataAge = *counter;
        }
        return data;
    }

    T operator()(void) const {
        // This operation is semantically const, so discard the actual
        // const
        return ((Cache*)this)->get();
    }
};

} // namespace llpm

#endif // __LLPM_UTIL_CACHE_HPP__
