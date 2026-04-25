#pragma once

#include <cstdint>

namespace omni::win {

  struct file_header {
    std::uint16_t machine;
    std::uint16_t num_sections;
    std::uint32_t timedate_stamp;
    std::uint32_t ptr_symbols;
    std::uint32_t num_symbols;
    std::uint16_t size_optional_header;
    std::uint16_t characteristics;
  };

} // namespace omni::win
