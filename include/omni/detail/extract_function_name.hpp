#pragma once

#include <string_view>

#include "omni/detail/config.hpp"

namespace omni::detail {

  template <auto Fn>
  consteval std::string_view extract_function_name() {
#if defined(OMNI_COMPILER_CLANG)
    // "... extract_function_name() [Fn = &FunctionNameA]"
    constexpr std::string_view pretty = __PRETTY_FUNCTION__;

    // '&' is where a function name begins
    constexpr auto name_start = pretty.rfind('&') + 1;
    constexpr auto name_end = pretty.find(']');
    constexpr auto func_name = pretty.substr(name_start, name_end - name_start);
#elif defined(OMNI_COMPILER_GCC)
    // "... extract_function_name() [with auto Fn = MessageBoxA; std::string_view = std::basic_string_view<char>]"
    constexpr std::string_view pretty = __PRETTY_FUNCTION__;

    constexpr std::string_view marker{" auto Fn = "};
    constexpr auto name_start = pretty.find(marker) + marker.size();
    constexpr auto name_end = pretty.find(';');
    constexpr auto func_name = pretty.substr(name_start, name_end - name_start);
#elif defined(OMNI_COMPILER_MSVC)
    // "... extract_function_name<int __cdecl A::B::FunctionNameA(int, int*)>(void)"
    constexpr std::string_view sig{__FUNCSIG__};
    constexpr std::string_view marker{"extract_function_name<"};

    constexpr std::size_t after = sig.find(marker) + marker.size(); // "... A::B::FunctionNameA("
    constexpr std::size_t paren = sig.find('(', after);             // '(' of param list
    constexpr auto left_part = sig.substr(after, paren - after);    // "int __cdecl A::B::FunctionNameA"

    // Points to last letter of the name
    constexpr std::size_t name_end = left_part.find_last_not_of(" \t");

    // Last whitespace before the name (space or tab) " A::B::FunctionNameA"
    constexpr std::size_t sep = left_part.find_last_of(" \t", name_end);

    // Begin of the (possibly qualified) identifier
    constexpr std::size_t name_begin = (sep == std::string_view::npos) ? 0 : sep + 1;

    // "A::B::FunctionNameA" or just "FunctionNameA"
    constexpr auto ident = left_part.substr(name_begin, name_end - name_begin + 1);

    // Drop scope qualifier (namespace/class) if present
    // (it will never be the case with WinAPI functions, but anyway...)
    constexpr std::size_t scope = ident.rfind("::");
    constexpr auto func_name = (scope == std::string_view::npos) ? ident : ident.substr(scope + 2);
#else
#  error Unsupported compiler
#endif
    static_assert(!func_name.empty(), "Failed to extract function name");
    return func_name;
  }

} // namespace omni::detail
