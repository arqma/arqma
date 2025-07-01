#pragma once
#include <type_traits>

namespace arqnet {

#ifdef __cpp_lib_void_t
using std::void_t;
#else
template <typename... Ts> struct void_t_impl { using type = void; };
template <typename... Ts> using void_t = typename void_t_impl<Ts...>::type;
#endif

};
