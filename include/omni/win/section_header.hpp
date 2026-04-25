#pragma once

#include <string_view>

namespace omni::win {

  struct section_name {
    constexpr static auto max_section_name_length{8};

    char name[max_section_name_length];

    [[nodiscard]] explicit operator std::string_view() const noexcept {
      return std::string_view{static_cast<const char*>(name)};
    }

    [[nodiscard]] auto operator[](size_t n) const noexcept {
      return static_cast<std::string_view>(*this)[n];
    }

    [[nodiscard]] bool operator==(const section_name& other) const {
      return std::string_view{*this} == std::string_view{other};
    }
  };

  struct section_header {
    section_name name;
    union {
      std::uint32_t physical_address;
      std::uint32_t virtual_size;
    };
    std::uint32_t virtual_address;

    std::uint32_t size_raw_data;
    std::uint32_t ptr_raw_data;

    std::uint32_t ptr_relocs;
    std::uint32_t ptr_line_numbers;
    std::uint16_t num_relocs;
    std::uint16_t num_line_numbers;

    std::uint32_t characteristics_flags;
  };

} // namespace omni::win
