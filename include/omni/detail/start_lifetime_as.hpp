#pragma once

#include <memory>

namespace omni::detail {

  template <typename T>
  [[nodiscard]] const T* start_lifetime_as(const void* location) noexcept {
#ifdef __cpp_lib_start_lifetime_as
    return std::start_lifetime_as<T>(location);
#else
    // Formally UB because no T object was created in the OS-populated
    // buffer, but this is the established fallback for standard libraries
    // without C++23 lifetime-management support
    return reinterpret_cast<const T*>(location);
#endif
  }

} // namespace omni::detail
