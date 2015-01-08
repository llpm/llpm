#ifndef __LLPM_MACROS_HPP__
#define __LLPM_MACROS_HPP__

template< class T > struct add_const_if_ref      {typedef T type;};
template< class T > struct add_const_if_ref<T&>  {typedef const T& type;};
template< class T > struct add_const_if_ref<T&&> {typedef const T&& type;};

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
    unsigned F##_size() const { return _##F.size(); } \
    auto F(unsigned i) const -> const decltype(_##F)::value_type { return _##F[i]; }


#endif // __LLPM_MACROS_HPP__
