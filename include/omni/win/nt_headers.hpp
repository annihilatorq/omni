#pragma once

#include "omni/win/file_header.hpp"
#include "omni/win/optional_header.hpp"
#include "omni/win/section_header.hpp"

namespace omni::win {

  struct nt_headers {
    std::uint32_t signature;
    win::file_header file_header;
    win::optional_header optional_header;

    [[nodiscard]] section_header* get_sections() {
      return reinterpret_cast<section_header*>(
        reinterpret_cast<std::byte*>(&optional_header) + file_header.size_optional_header);
    }

    [[nodiscard]] section_header* get_section(size_t n) {
      return n >= file_header.num_sections ? nullptr : get_sections() + n;
    }

    [[nodiscard]] const section_header* get_sections() const {
      return reinterpret_cast<const section_header*>(
        reinterpret_cast<const std::byte*>(&optional_header) + file_header.size_optional_header);
    }

    [[nodiscard]] const section_header* get_section(size_t n) const {
      return n >= file_header.num_sections ? nullptr : get_sections() + n;
    }

    template <typename T>
    struct section_iterator {
      T* base;
      uint16_t count;

      [[nodiscard]] T* begin() const {
        return base;
      }

      [[nodiscard]] T* end() const {
        return base + count;
      }
    };

    [[nodiscard]] section_iterator<section_header> sections() {
      return {.base = get_sections(), .count = file_header.num_sections};
    }
    [[nodiscard]] section_iterator<const section_header> sections() const {
      return {.base = get_sections(), .count = file_header.num_sections};
    }
  };

} // namespace omni::win
