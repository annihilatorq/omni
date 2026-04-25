#pragma once

#include <cstdint>
#include <type_traits>

#include "omni/detail/config.hpp"
#include "omni/win/directories.hpp"
#include "omni/win/version.hpp"

namespace omni::win {

  enum class subsystem_id : std::uint16_t {
    unknown = 0x0000,        // Unknown subsystem.
    native = 0x0001,         // Image doesn't require a subsystem.
    windows_gui = 0x0002,    // Image runs in the Windows GUI subsystem.
    windows_cui = 0x0003,    // Image runs in the Windows character subsystem
    os2_cui = 0x0005,        // image runs in the OS/2 character subsystem.
    posix_cui = 0x0007,      // image runs in the Posix character subsystem.
    native_windows = 0x0008, // image is a native Win9x driver.
    windows_ce_gui = 0x0009, // Image runs in the Windows CE subsystem.
    efi_application = 0x000A,
    efi_boot_service_driver = 0x000B,
    efi_runtime_driver = 0x000C,
    efi_rom = 0x000D,
    xbox = 0x000E,
    windows_boot_application = 0x0010,
    xbox_code_catalog = 0x0011,
  };

  struct optional_header_x64 {
    std::uint16_t magic;
    version linker_version;
    std::uint32_t size_code;
    std::uint32_t size_init_data;
    std::uint32_t size_uninit_data;
    std::uint32_t entry_point;
    std::uint32_t base_of_code;
    std::uint64_t image_base;
    std::uint32_t section_alignment;
    std::uint32_t file_alignment;
    ex_version os_version;
    ex_version img_version;
    ex_version subsystem_version;
    std::uint32_t win32_version_value;
    std::uint32_t size_image;
    std::uint32_t size_headers;
    std::uint32_t checksum;
    subsystem_id subsystem;
    std::uint16_t characteristics;
    std::uint64_t size_stack_reserve;
    std::uint64_t size_stack_commit;
    std::uint64_t size_heap_reserve;
    std::uint64_t size_heap_commit;
    std::uint32_t ldr_flags;
    std::uint32_t num_data_directories;
    data_directories_x64 data_directories;
  };

  struct optional_header_x86 {
    std::uint16_t magic;
    version linker_version;
    std::uint32_t size_code;
    std::uint32_t size_init_data;
    std::uint32_t size_uninit_data;
    std::uint32_t entry_point;
    std::uint32_t base_of_code;
    std::uint32_t base_of_data;
    std::uint32_t image_base;
    std::uint32_t section_alignment;
    std::uint32_t file_alignment;
    ex_version os_version;
    ex_version img_version;
    ex_version subsystem_version;
    std::uint32_t win32_version_value;
    std::uint32_t size_image;
    std::uint32_t size_headers;
    std::uint32_t checksum;
    subsystem_id subsystem;
    std::uint16_t characteristics;
    std::uint32_t size_stack_reserve;
    std::uint32_t size_stack_commit;
    std::uint32_t size_heap_reserve;
    std::uint32_t size_heap_commit;
    std::uint32_t ldr_flags;
    std::uint32_t num_data_directories;
    data_directories_x86 data_directories;

    [[nodiscard]] bool has_directory(const data_directory* dir) const {
      if (dir == nullptr) {
        return false;
      }

      return &data_directories.entries[num_data_directories] < dir && dir->present();
    }

    [[nodiscard]] bool has_directory(directory_id id) const {
      return has_directory(&data_directories.entries[id]);
    }
  };

  using optional_header = std::conditional_t<detail::is_x64, optional_header_x64, optional_header_x86>;

} // namespace omni::win
