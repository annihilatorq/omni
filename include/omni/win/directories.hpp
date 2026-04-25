#pragma once

#include <cstdint>

#include "omni/win/version.hpp"

namespace omni::win {

  constexpr inline std::uint32_t num_data_directories = 16U;

  enum directory_id : std::uint8_t {
    directory_entry_export = 0,          // Export Directory
    directory_entry_import = 1,          // Import Directory
    directory_entry_resource = 2,        // Resource Directory
    directory_entry_exception = 3,       // Exception Directory
    directory_entry_security = 4,        // Security Directory
    directory_entry_basereloc = 5,       // Base Relocation Table
    directory_entry_debug = 6,           // Debug Directory
    directory_entry_copyright = 7,       // (X86 usage)
    directory_entry_architecture = 7,    // Architecture Specific Data
    directory_entry_globalptr = 8,       // RVA of GP
    directory_entry_tls = 9,             // TLS Directory
    directory_entry_load_config = 10,    // Load Configuration Directory
    directory_entry_bound_import = 11,   // Bound Import Directory in headers
    directory_entry_iat = 12,            // Import Address Table
    directory_entry_delay_import = 13,   // Delay Load Import Descriptors
    directory_entry_com_descriptor = 14, // COM Runtime descriptor
    directory_reserved0 = 15,            // -
  };

  struct data_directory {
    std::uint32_t rva;
    std::uint32_t size;

    [[nodiscard]] bool present() const noexcept {
      return size > 0;
    }
  };

  struct raw_data_directory {
    std::uint32_t ptr_raw_data;
    std::uint32_t size;

    [[nodiscard]] bool present() const noexcept {
      return size > 0;
    }
  };

  struct data_directories_x64 {
    union {
      struct {
        data_directory export_directory;
        data_directory import_directory;
        data_directory resource_directory;
        data_directory exception_directory;
        raw_data_directory security_directory; // File offset instead of RVA!
        data_directory basereloc_directory;
        data_directory debug_directory;
        data_directory architecture_directory;
        data_directory globalptr_directory;
        data_directory tls_directory;
        data_directory load_config_directory;
        data_directory bound_import_directory;
        data_directory iat_directory;
        data_directory delay_import_directory;
        data_directory com_descriptor_directory;
        data_directory _reserved0;
      };
      data_directory entries[num_data_directories];
    };
  };

  struct data_directories_x86 {
    union {
      struct {
        data_directory export_directory;
        data_directory import_directory;
        data_directory resource_directory;
        data_directory exception_directory;
        raw_data_directory security_directory; // File offset instead of RVA!
        data_directory basereloc_directory;
        data_directory debug_directory;
        data_directory copyright_directory;
        data_directory globalptr_directory;
        data_directory tls_directory;
        data_directory load_config_directory;
        data_directory bound_import_directory;
        data_directory iat_directory;
        data_directory delay_import_directory;
        data_directory com_descriptor_directory;
        data_directory _reserved0;
      };
      data_directory entries[num_data_directories];
    };
  };

  struct export_directory {
    std::uint32_t characteristics;
    std::uint32_t timedate_stamp;
    win::version version;
    std::uint32_t name;
    std::uint32_t base;
    std::uint32_t num_functions;
    std::uint32_t num_names;
    std::uint32_t rva_functions;
    std::uint32_t rva_names;
    std::uint32_t rva_name_ordinals;

    [[nodiscard]] auto rva_table(std::uintptr_t base_address) const {
      return reinterpret_cast<std::uint32_t*>(base_address + rva_functions);
    }

    [[nodiscard]] auto ordinal_table(std::uintptr_t base_address) const {
      return reinterpret_cast<std::uint16_t*>(base_address + rva_name_ordinals);
    }
  };

} // namespace omni::win
