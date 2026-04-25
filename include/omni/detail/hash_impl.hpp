#pragma once

#include "omni/concepts.hpp"

namespace omni::detail {

  template <std::unsigned_integral T>
  class fnv1a_hash {
    constexpr static auto FNV_prime = (sizeof(T) == 4) ? 16777619ULL : 1099511628211ULL;
    constexpr static auto FNV_offset_basis = (sizeof(T) == 4) ? 2166136261ULL : 14695981039346656037ULL;

   public:
    constexpr static auto initial_value = FNV_offset_basis;
    using value_type = T;

    constexpr fnv1a_hash() = default;

    constexpr explicit(false) fnv1a_hash(value_type value): value_(value) {}

    // Implicit constructor is key here. It allows passing string literals in
    // parameter-list where basic_hash type is expected, using this implicit
    // consteval constructor we perform compile-time string hashing without
    // forcing user to specify hash type, so instead of writing this:
    // `foo(omni::hash_name{"string"})`
    // user will need to write:
    // `foo("string")`
    // while still achieving absolutely the same result and still being able
    // to specify their own hashing policy
    template <typename CharT, std::size_t N>
    consteval explicit(false) fnv1a_hash(const CharT (&string)[N]) {
      for (std::size_t i{}; i < N - 1; i++) {
        value_ = fnv1a_append_bytes<CharT>(value_, string[i]);
      }
    }

    consteval explicit(false) fnv1a_hash(concepts::hashable auto string) {
      for (std::size_t i{}; i < string.size(); i++) {
        value_ = fnv1a_append_bytes<>(value_, string[i]);
      }
    }

    [[nodiscard]] value_type operator()(concepts::hashable auto object) {
      T value{FNV_offset_basis};
      for (std::size_t i{}; i < object.size(); i++) {
        value = fnv1a_append_bytes<>(value, object[i]);
      }
      return value;
    }

    [[nodiscard]] value_type value() const {
      return value_;
    }

    [[nodiscard]] constexpr auto operator<=>(const fnv1a_hash&) const = default;

    [[nodiscard]] constexpr auto operator<=>(value_type other) const {
      return value_ <=> other;
    }

    [[nodiscard]] constexpr bool operator==(value_type other) const {
      return value_ == other;
    }

    [[nodiscard]] constexpr bool operator==(const fnv1a_hash& other) const {
      return value_ == other.value_;
    }

    friend std::ostream& operator<<(std::ostream& os, const fnv1a_hash& hash) {
      return os << hash.value();
    }

   private:
    template <typename CharT>
    [[nodiscard]] constexpr static value_type fnv1a_append_bytes(value_type accumulator, CharT byte) {
      accumulator ^= static_cast<value_type>(to_lower(byte));
      accumulator *= FNV_prime;
      return accumulator;
    }

    template <typename CharT>
    [[nodiscard]] constexpr static CharT to_lower(CharT c) {
      return ((c >= 'A' && c <= 'Z') ? (c + 32) : c);
    }

    value_type value_{FNV_offset_basis};
  };

  static_assert(concepts::hash<fnv1a_hash<std::uint32_t>>);
  static_assert(concepts::hash<fnv1a_hash<std::uint64_t>>);

} // namespace omni::detail
