#pragma once

#include <cstddef>
#include <cstdint>

#include "omni/address.hpp"

namespace omni::win {

  // https://www.geoffchappell.com/studies/windows/km/ntoskrnl/api/rtl/ldrreloc/process_module_information.htm
  struct system_module_information {
    omni::address section;
    omni::address mapped_base;
    omni::address image_base;
    std::uint32_t image_size;
    std::uint32_t flags;
    std::uint16_t load_order_index;
    std::uint16_t init_order_index;
    std::uint16_t load_count;
    std::uint16_t offset_to_file_name;
    char full_path_name[256];
  };

  // Layout of the SystemModuleInformation (11) query result.
  struct system_modules_information { // NOLINT(*-member-init)
    std::uint32_t number_of_modules;
    system_module_information modules[1];
  };

  static_assert(sizeof(system_module_information) == (sizeof(void*) == 8 ? 0x128 : 0x11C));
  static_assert(offsetof(system_module_information, image_base) == (sizeof(void*) == 8 ? 0x10 : 0x08));
  static_assert(offsetof(system_module_information, image_size) == (sizeof(void*) == 8 ? 0x18 : 0x0C));
  static_assert(offsetof(system_module_information, full_path_name) == (sizeof(void*) == 8 ? 0x28 : 0x1C));
  static_assert(offsetof(system_modules_information, modules) == (sizeof(void*) == 8 ? 0x08 : 0x04));

} // namespace omni::win
