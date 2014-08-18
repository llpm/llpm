#ifndef __LLPM_PRIORITY_COLLECTION_HPP__
#define __LLPM_PRIORITY_COLLECTION_HPP__

#include <iostream>
#include <type_traits>
#include <typeindex>
#include <vector>
#include <unordered_map>
#include <boost/foreach.hpp>
#include <util/macros.hpp>

namespace llpm {
    
template<class V>
class PCHandler {
public:
    // Determines weather or not 
    virtual bool handles(V* v) const = 0;
};

template<typename V>
class Library {
protected:
    std::vector<V*> _collection;
    Library(std::vector<V*> collection) :
        _collection(collection)
    { }

public:
    const std::vector<V*>& collection() {
        return _collection;
    }

    virtual std::string name() = 0;
};

/********
 * Contains an ordered list of widgets with particular capabilities,
 * the order defining the priorities of the widgets. When a lookup
 * is executed, a list of widgets with the correct capabilities is
 * returned, maintaining priority order.
 */
template<class K, class V>
class PriorityCollection {
public:
    // V, the library entries, must extend Handler 
#if 0
    static_assert(std::is_base_of<PCHandler<K>, V>::value,
                  "PriorityCollection contents must extend PCHandler class");
#endif

private:
    std::vector<V*> _entries;
    std::unordered_map<std::type_index, std::vector<V*> > _cache;
    std::vector< Library<V>* > _libraries;

public:
    PriorityCollection() { }
    PriorityCollection(const std::vector<V*>& entries) {
        this->setEntries(entries);
    }
    ~PriorityCollection() { }

    const std::vector<V*>& entries() const {
        return entries;
    }
    void setEntries(const std::vector<V*>& entries) {
        _entries = entries;
        _cache.clear();
    }

    void prependEntry(V* entry) {
        _entries.push_front(entry);
        _cache.clear();
    }

    void prependEntries(const std::vector<V*>& entries) {
        _entries.insert(_entries.begin(), entries.begin(), entries.end());
        _cache.clear();
    }

    void appendEntry(V* entry) {
        _entries.push_back(entry);
        _cache.clear();
    }

    void appendEntries(const std::vector<V*>& entries) {
        _entries.insert(_entries.end(), entries.begin(), entries.end());
        _cache.clear();
    }

    void appendLibrary(Library<V>* lib) {
        _libraries.push_back(lib);
        appendEntries(lib->collection());
    }

    void prependLibrary(Library<V>* lib) {
        _libraries.push_front(lib);
        prependEntries(lib->collection());
    }

    const std::vector<V*>& lookup(K* k) {
        std::type_index ti = typeid(*k);
        auto f = _cache.find(ti);
        if (f == _cache.end()) {
            std::cout << "Non cache for: " << typeid(*k).name() << std::endl;
            std::vector<V*>& vvec = _cache[ti];
            BOOST_FOREACH(auto v, _entries) {
                if (v->handles(k)) {
                    vvec.push_back(v);
                }
            }
            return vvec;
        } else {
            return f->second;
        }
    }

    const std::vector<V*>& operator()(K* k) {
        return lookup(k);
    }
};

} // namespace llpm

#endif // __LLPM_PRIORITY_COLLECTION_HPP__
