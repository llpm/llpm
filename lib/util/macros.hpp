#ifndef __LLPM_MACROS_HPP__
#define __LLPM_MACROS_HPP__

/***********
 * Methods to create setters and getters
 */
#define DEF_SET(F) void F( decltype(_##F) f ) { _##F = f; }
#define DEF_SET_NONULL(F) void F( decltype(_##F) f ) { \
    if (f == NULL) throw NullException(); \
    _##F = f; }
#define DEF_GET(F) auto F() const -> const decltype(_##F)* { return &_##F; }
#define DEF_ARRAY_GET(F) \
    unsigned F##_size() { return _##F.size(); } \
    auto F(unsigned i) const -> const decltype(_##F)::value_type* { return &_##F[i]; }


#endif // __LLPM_MACROS_HPP__
