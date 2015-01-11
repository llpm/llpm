#ifndef __LLPM_UTIL_CACHE_HPP__
#define __LLPM_UTIL_CACHE_HPP__

#include <stdint.h>
#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <map>

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
    
    const T& get() {
        assert(initialized);
        if (*counter != dataAge) {
            data = update();
            dataAge = *counter;
        }
        return data;
    }

    const T& operator()(void) const {
        // This operation is semantically const, so discard the actual
        // const
        return ((Cache*)this)->get();
    }
};

template<typename K, typename T>
class CacheMap {
    bool initialized;
    std::map<K, std::pair<uint64_t, T>> data;
    const uint64_t* counter;
    boost::function<T(K)> update;

public:
    CacheMap() : initialized(false) {
    }

    CacheMap(const uint64_t* counter,
          boost::function<T(K)> update) :
        initialized(true),
        counter(counter),
        update(update)
    { }

    void setup(const uint64_t* counter,
               boost::function<T(void)> update) {
        this->counter = counter;
        this->update = update;
        this->initialized = true;
   }

    void reset() {
        data.clear();
    }
    
    const T& get(K k) {
        assert(initialized);
        if (data.find(k) == data.end()) {
            data[k] = std::make_pair(*counter, update(k));
            return data[k].second;
        } else {
            auto& p = data[k];
            if (*counter != p.first) {
                p = std::make_pair(*counter, update(k));
            }
            return p.second;
        }
    }

    const T& operator()(K k) const {
        // This operation is semantically const, so discard the actual
        // const
        return ((CacheMap*)this)->get(k);
    }
};

} // namespace llpm

#endif // __LLPM_UTIL_CACHE_HPP__
