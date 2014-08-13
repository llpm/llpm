#ifndef __LLPM_PRIORITY_COLLECTION_HPP__
#define __LLPM_PRIORITY_COLLECTION_HPP__

#include <type_traits>
#include <vector>
#include <unordered_map>
#include <boost/foreach.hpp>

namespace llpm {
    
template<class K>
class PCHandler {
public:
    // Determines weather or not 
    virtual bool handles(K k) const = 0;
};

/********
 * Contains an ordered list of widgets with particular capabilities,
 * the order defining the priorities of the widgets. When a lookup
 * is executed, the highest priority widget with the necessary
 * capabilities is selected.
 */
template<class K, class V>
class PriorityCollection {
public:
    // V, the library entries, must extend Handler 
#if 0
    static_assert(std::is_trivially_assignable<V, PCHandler<K> >::value,
                  "PriorityCollection contents must extend PCHandler class");
#endif

private:
    std::vector<V*> _entries;
    std::unordered_map<K, V*> _cache;

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

    void appendEntry(V* entry) {
        _entries.push_back(entry);
        _cache.clear();
    }

    void appendEntries(const std::vector<V*>& entries) {
        _entries.insert(_entries.back(), entries.front(), entries.back());
        _cache.clear();
    }

    V* lookup(K k) {
        auto f = _cache.find(k);
        if (f == _cache.end()) {
            BOOST_FOREACH(auto v, _entries) {
                if (v->handles(k)) {
                    _cache[k] = v;
                    return v;
                }
            }
            _cache[k] = NULL;
            return NULL;
        } else {
            return f->second;
        }
    }

    V* operator()(K k) {
        return lookup(k);
    }
};

} // namespace llpm

#endif // __LLPM_PRIORITY_COLLECTION_HPP__
