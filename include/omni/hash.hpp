#pragma once

#include <cstdint>
#include <format>

#include "omni/concepts/concepts.hpp"
#include "omni/detail/hash_impl.hpp"

namespace omni {

  using fnv1a32 = detail::fnv1a_hash<std::uint32_t>;
  using fnv1a64 = detail::fnv1a_hash<std::uint64_t>;
  using default_hash = fnv1a64;

  template <concepts::hash Hasher = default_hash>
  struct hash_pair {
    consteval hash_pair(Hasher first, Hasher second): first(first), second(second) {}

    Hasher first;
    Hasher second;
  };

  static_assert(concepts::hash<fnv1a32>);
  static_assert(concepts::hash<fnv1a64>);
  static_assert(concepts::hash<default_hash>);

  template <typename T>
  [[nodiscard]] constexpr auto hash(concepts::hashable auto object) {
    return T{}(object);
  }

  namespace literals {
    consteval omni::fnv1a32 operator""_fnv1a32(const char* str, std::size_t len) noexcept {
      return {std::string_view{str, len}};
    }

    consteval omni::fnv1a64 operator""_fnv1a64(const char* str, std::size_t len) noexcept {
      return {std::string_view{str, len}};
    }

    consteval omni::default_hash operator""_hash(const char* str, std::size_t len) noexcept {
      return {std::string_view{str, len}};
    }

    consteval omni::fnv1a32 operator""_fnv1a32(const wchar_t* str, std::size_t len) noexcept {
      return {std::wstring_view{str, len}};
    }

    consteval omni::fnv1a64 operator""_fnv1a64(const wchar_t* str, std::size_t len) noexcept {
      return {std::wstring_view{str, len}};
    }

    consteval omni::default_hash operator""_hash(const wchar_t* str, std::size_t len) noexcept {
      return {std::wstring_view{str, len}};
    }
  } // namespace literals

} // namespace omni

template <>
struct std::formatter<omni::fnv1a32> : std::formatter<omni::fnv1a32::value_type> {
  auto format(const omni::fnv1a32& hash, std::format_context& ctx) const {
    return std::formatter<omni::fnv1a32::value_type>::format(hash.value(), ctx);
  }
};

template <>
struct std::formatter<omni::fnv1a64> : std::formatter<omni::fnv1a64::value_type> {
  auto format(const omni::fnv1a64& hash, std::format_context& ctx) const {
    return std::formatter<omni::fnv1a64::value_type>::format(hash.value(), ctx);
  }
};
