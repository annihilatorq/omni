#pragma once

#include "omni/address.hpp"
#include "omni/win/kernel_user_shared_data.hpp"

namespace omni {

  [[nodiscard]] inline omni::win::kernel_user_shared_data* shared_user_data() noexcept {
    constexpr static omni::address memory_location{0x7ffe0000};
    return memory_location.ptr<win::kernel_user_shared_data>();
  }

} // namespace omni
