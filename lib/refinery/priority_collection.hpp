#ifndef __LLPM_PRIORITY_COLLECTION_HPP__
#define __LLPM_PRIORITY_COLLECTION_HPP__

#include <type_traits>
#include <vector>
#include <unordered_map>
#include <boost/foreach.hpp>

namespace llpm {
    
template<class V>
class PCHandler {
public:
    // Determines weather or not 
    virtual bool handles(V v) const = 0;
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
    static_assert(std::is_base_of<PCHandler<K>, V>::value,
                  "PriorityCollection contents must extend PCHandler class");

private:
    std::vector<V*> _entries;
    std::unordered_map<K, std::vector<V*> > _cache;

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
        _entries.insert(_entries.front(), entries.front(), entries.back());
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

    const std::vector<V*>& lookup(K k) {
        auto f = _cache.find(k);
        if (f == _cache.end()) {
            std::vector<V*>& vvec = _cache[k];
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

    const std::vector<V*>& operator()(K k) {
        return lookup(k);
    }
};

} // namespace llpm

#endif // __LLPM_PRIORITY_COLLECTION_HPP__
