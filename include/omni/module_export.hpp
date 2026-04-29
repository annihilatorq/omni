#pragma once

#include <array>
#include <cassert>
#include <charconv>
#include <cstdint>
#include <format>
#include <limits>
#include <string_view>

#include "omni/address.hpp"
#include "omni/api_set.hpp"

namespace omni {

  struct use_ordinal_t {};
  [[maybe_unused]] constexpr inline use_ordinal_t use_ordinal{};

  struct forwarder_string {
    std::string_view module;
    std::string_view function;

    [[nodiscard]] static forwarder_string parse(std::string_view forwarder_str) noexcept {
      auto pos = forwarder_str.find('.');
      if (pos != std::string_view::npos) {
        auto first_part = forwarder_str.substr(0, pos);
        auto second_part = forwarder_str.substr(pos + 1);
        return forwarder_string{.module = first_part, .function = second_part};
      }
      assert(false);
      return forwarder_string{.module = forwarder_str, .function = std::string_view{}};
    }

    [[nodiscard]] bool is_ordinal() const noexcept {
      return !function.empty() && function.front() == '#';
    }

    [[nodiscard]] std::uint32_t to_ordinal() const noexcept {
      if (function.empty()) {
        return 0;
      }

      std::uint32_t ordinal{};
      // Ordinal forwarder always starts from '#', skip it
      auto ordinal_str = function.substr(1);
      [[maybe_unused]] auto result = std::from_chars(ordinal_str.data(), ordinal_str.data() + ordinal_str.size(), ordinal);
      assert(result.ec == std::errc{});
      return ordinal;
    }

    [[nodiscard]] bool present() const noexcept {
      return !(module.empty() || function.empty());
    }
  };

  struct named_export {
    std::string_view name;
    omni::address address;
    omni::forwarder_string forwarder_string{};
    std::optional<omni::api_set> forwarder_api_set;
    omni::address module_base;

    [[nodiscard]] bool is_forwarded() const noexcept {
      return forwarder_string.present();
    }

    [[nodiscard]] bool is_ordinal_only() const noexcept {
      return name.empty();
    }

    [[nodiscard]] bool present() const noexcept {
      return static_cast<bool>(address);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return present();
    }
  };

  struct ordinal_export {
    std::uint32_t ordinal{};
    omni::address address;
    omni::forwarder_string forwarder_string{};
    std::optional<omni::api_set> forwarder_api_set;
    omni::address module_base;

    [[nodiscard]] bool is_forwarded() const noexcept {
      return forwarder_string.present();
    }

    [[nodiscard]] bool present() const noexcept {
      return static_cast<bool>(address);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return present();
    }
  };

} // namespace omni

template <>
struct std::formatter<omni::named_export> : std::formatter<std::string_view> {
  auto format(const omni::named_export& named_export, std::format_context& ctx) const {
    return std::formatter<std::string_view, char>::format(named_export.name, ctx);
  }
};

template <>
struct std::formatter<omni::ordinal_export> : std::formatter<std::string_view> {
  auto format(const omni::ordinal_export& ordinal_export, std::format_context& ctx) const {
    if (!ordinal_export.present()) {
      return ctx.out();
    }

    // Zero-allocation path for ordinal exports, since the formatter is
    // required to write data from the view to the format_context::out()
    // before exiting the scope
    std::array<char, std::numeric_limits<std::uint32_t>::digits> ordinal_buf{};
    ordinal_buf[0] = '#';

    auto conversion_result =
      std::to_chars(ordinal_buf.data() + 1, ordinal_buf.data() + ordinal_buf.size(), ordinal_export.ordinal);
    std::size_t digits_converted = conversion_result.ptr - ordinal_buf.data();

    std::string_view ordinal_view(ordinal_buf.data(), digits_converted);
    return std::formatter<std::string_view, char>::format(ordinal_view, ctx);
  }
};
