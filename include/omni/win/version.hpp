#pragma once

#include <cstdint>

namespace omni::win {

  union version {
    std::uint16_t identifier;
    struct {
      std::uint8_t major;
      std::uint8_t minor;
    };
  };

  union ex_version {
    std::uint32_t identifier;
    struct {
      std::uint16_t major;
      std::uint16_t minor;
    };
  };

} // namespace omni::win
