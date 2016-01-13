#pragma once

#include <iostream>
#include <sstream>
#include <string>
#include <algorithm>
#include <memory>
#include <type_traits>

template< class T > struct add_const_if_ref      {typedef T type;};
template< class T > struct add_const_if_ref<T&>  {typedef const T& type;};
template< class T > struct add_const_if_ref<T&&> {typedef const T&& type;};

template <typename T>
struct RawEqualTo
{
    RawEqualTo(T const* raw) : raw(raw) {}

    bool operator()(T const* p) const  
        { return raw == p; }

    template <typename T1, typename D1>
    bool operator()(const std::unique_ptr<T1, D1>& up) const  
        { return raw == up.get(); }

  private:
    T const* raw;
};

/***********
 * Methods to create setters and getters
 */
#define DEF_SET(F) void F( decltype(_##F) f ) { _##F = f; }
#define DEF_SET_NONULL(F) void F( decltype(_##F) f ) { \
    if (f == NULL) throw NullException(); \
    _##F = f; }

#define DEF_GET(F) \
    auto F() const -> const decltype(_##F)* { return &_##F; } \
    auto F() -> decltype(_##F)* { return &_##F; }

#define DEF_GET_NP(F) \
    auto F() const -> add_const_if_ref<decltype(_##F)>::type { return _##F; } \
    auto F() -> decltype(_##F) { return _##F; }

#define DEF_ARRAY_GET(F) \
    auto F() const -> const decltype(_##F)& { return _##F; } \
    unsigned F##_size() const { return _##F.size(); } \
    auto F(unsigned i) const -> const decltype(_##F)::value_type& { \
        return _##F[i]; }

#define DEF_UNIQ_ARRAY_GET(F) \
    void F(std::vector<const decltype(_##F):: \
                       value_type::element_type*>& ret) const { \
        for (auto&& up: _##F) ret.push_back(up.get()); \
    } \
    void F(std::vector<decltype(_##F):: \
                       value_type::element_type*>& ret) { \
        for (auto&& up: _##F) ret.push_back(up.get()); \
    } \
    unsigned F##_size() const { return _##F.size(); } \
    auto F(unsigned i) const -> \
        decltype(_##F)::value_type::element_type* { \
        return _##F[i].get(); }

#define DEL_IF(A) \
    if ((A) != NULL) delete (A);

#define DEL_ARRAY(A) \
    { for (auto a: A) { delete a; } }


// enable operator<< and operator>> for V values
#define ENUM_SER(V, STRINGS) \
    std::ostream& operator<<(std::ostream& str, const V& data) { \
       return str << STRINGS[static_cast<unsigned>(data)]; \
    } \
    std::istream& operator>>(std::istream& str, V& data) { \
        std::string value; \
        str >> value; \
        static auto begin  = std::begin(STRINGS); \
        static auto end    = std::end(STRINGS); \
        auto find = std::find(begin, end, value); \
        if (find != end) {    \
            data = static_cast<V>(std::distance(begin, find)); \
        } \
        return str; \
    }

