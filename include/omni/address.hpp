#pragma once

#include <concepts>
#include <cstdint>
#include <format>
#include <iostream>
#include <optional>
#include <span>
#include <variant>

#include "omni/concepts.hpp"

namespace omni {

  class address {
   public:
    using value_type = std::uintptr_t;
    using difference_type = std::ptrdiff_t;

    constexpr address() = default;

    // This way, we create a perfect-match ctor, eliminating any ambiguity for
    // the compiler when choosing between the pointer and nullptr for the ctor
    // (since compiler is allowed to perform one user-defined conversion)
    constexpr explicit address(concepts::nullpointer auto) noexcept {}

    constexpr explicit(false) address(value_type address) noexcept: address_(address) {}
    constexpr explicit address(concepts::pointer auto ptr) noexcept: address_(reinterpret_cast<value_type>(ptr)) {}
    constexpr explicit address(std::ranges::contiguous_range auto range) noexcept
      : address_(reinterpret_cast<value_type>(range.data())) {}

    address(const address&) = default;
    address(address&&) = default;

    address& operator=(const address&) = default;
    address& operator=(address&&) = default;

    address& operator=(std::nullptr_t) {
      address_ = 0;
      return *this;
    }

    address& operator=(concepts::pointer auto ptr) {
      address_ = reinterpret_cast<value_type>(ptr);
      return *this;
    }

    ~address() = default;

    template <typename T = void, typename PointerT = std::add_pointer_t<T>>
    [[nodiscard]] constexpr PointerT ptr(difference_type offset = 0) const noexcept {
      return this->offset(offset).as<PointerT>();
    }

    [[nodiscard]] constexpr value_type value() const noexcept {
      return address_;
    }

    template <concepts::pointer T>
    [[nodiscard]] constexpr T offset(difference_type offset = 0) const noexcept {
      return address_ == 0U ? nullptr : reinterpret_cast<T>(address_ + offset);
    }

    template <typename T = address>
    [[nodiscard]] constexpr T offset(difference_type offset = 0) const noexcept {
      return address_ == 0U ? static_cast<T>(*this) : T{address_ + offset};
    }

    template <concepts::pointer T>
    [[nodiscard]] constexpr T as() const noexcept {
      return reinterpret_cast<T>(address_);
    }

    template <std::convertible_to<value_type> T>
    [[nodiscard]] constexpr T as() const noexcept {
      return static_cast<T>(address_);
    }

    template <typename T, std::size_t Extent = std::dynamic_extent>
    [[nodiscard]] constexpr std::span<T, Extent> span(std::size_t count) const noexcept {
      return {this->ptr<T>(), count};
    }

    [[nodiscard]] bool is_in_range(address start, address end) const noexcept {
      return (*this >= start) && (*this < end);
    }

    template <typename T = std::monostate, typename... Args>
    [[nodiscard]] auto invoke(Args&&... args) const noexcept {
      using target_function_t = T(__stdcall*)(std::decay_t<Args>...);

      if (address_ == 0) {
        if constexpr (std::is_void_v<T>) {
          return false;
        } else {
          return std::optional<T>{};
        }
      }

      const auto target_function = reinterpret_cast<target_function_t>(address_);

      if constexpr (std::is_void_v<T>) {
        target_function(std::forward<Args>(args)...);
        return true;
      } else {
        return std::optional<T>{target_function(std::forward<Args>(args)...)};
      }
    }

    constexpr explicit operator std::uintptr_t() const noexcept {
      return address_;
    }

    constexpr explicit operator bool() const noexcept {
      return static_cast<bool>(address_);
    }

    constexpr auto operator<=>(const address&) const = default;

    [[nodiscard]] bool operator==(const address& other) const noexcept {
      return address_ == other.address_;
    }

    [[nodiscard]] bool operator==(concepts::pointer auto ptr) const noexcept {
      return *this == address{ptr};
    }

    [[nodiscard]] bool operator==(concepts::nullpointer auto) const noexcept {
      return address_ == 0;
    }

    [[nodiscard]] bool operator==(value_type value) const noexcept {
      return address_ == value;
    }

    constexpr address& operator+=(const address& rhs) noexcept {
      address_ += rhs.address_;
      return *this;
    }

    constexpr address& operator-=(const address& rhs) noexcept {
      address_ -= rhs.address_;
      return *this;
    }

    [[nodiscard]] constexpr address operator+(const address& rhs) const noexcept {
      return address{address_ + rhs.address_};
    }

    [[nodiscard]] constexpr address operator-(const address& rhs) const noexcept {
      return address{address_ - rhs.address_};
    }

    [[nodiscard]] constexpr address operator&(const address& other) const noexcept {
      return address{address_ & other.address_};
    }

    [[nodiscard]] constexpr address operator|(const address& other) const noexcept {
      return address{address_ | other.address_};
    }

    [[nodiscard]] constexpr address operator^(const address& other) const noexcept {
      return address{address_ ^ other.address_};
    }

    [[nodiscard]] constexpr address operator<<(std::size_t shift) const noexcept {
      return address{address_ << shift};
    }

    [[nodiscard]] constexpr address operator>>(std::size_t shift) const noexcept {
      return address{address_ >> shift};
    }

    friend std::ostream& operator<<(std::ostream& os, const address& address) {
      return os << address.ptr();
    }

   private:
    value_type address_{0};
  };

} // namespace omni

template <>
struct std::formatter<omni::address> : std::formatter<omni::address::value_type> {
  auto format(const omni::address& address, std::format_context& ctx) const {
    return std::formatter<omni::address::value_type>::format(address.value(), ctx);
  }
};
