#pragma once

#include <concepts>
#include <span>
#include <string_view>
#include <type_traits>

namespace omni::concepts {

  template <typename T> concept arithmetic = std::is_arithmetic_v<T>;
  template <typename T> concept pointer = std::is_pointer_v<T>;
  template <typename T> concept nullpointer = std::is_null_pointer_v<T>;

  template <typename T>
  concept function_pointer =
    std::is_pointer_v<std::remove_cvref_t<T>> && std::is_function_v<std::remove_pointer_t<std::remove_cvref_t<T>>>;

  template <typename T>
  concept hashable = requires(T t) {
    { t.size() } -> std::convertible_to<std::size_t>;
    { t[0] } -> std::convertible_to<typename T::value_type>;
  };

  template <typename T>
  concept hash = requires(T hasher, const char (&ct_string)[1], const wchar_t (&ct_wstring)[1]) {
    { hasher(std::string_view{}) } -> std::same_as<typename T::value_type>;
    { hasher(std::wstring_view{}) } -> std::same_as<typename T::value_type>;
    { hasher(std::span<char>{}) } -> std::same_as<typename T::value_type>;
    { hasher(std::span<wchar_t>{}) } -> std::same_as<typename T::value_type>;

    { hasher.value() } -> std::same_as<typename T::value_type>;

    hasher == hasher;
    hasher == typename T::value_type{};

    T{};
    T{ct_string};
    T{ct_wstring};
    T{std::string_view{}};
    T{typename T::value_type{}};
  };

} // namespace omni::concepts
