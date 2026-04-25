#pragma once

#include <iterator>
#include <string_view>

namespace omni::detail {

  template <std::size_t N>
  struct fixed_string {
    char value[N]{};

    consteval explicit(false) fixed_string(const char (&str)[N]) {
      for (std::size_t i = 0; i < N; ++i) {
        value[i] = str[i];
      }
    }

    [[nodiscard]] constexpr std::string_view view() const {
      return std::string_view{std::data(value), N - 1};
    }
  };

} // namespace omni::detail
