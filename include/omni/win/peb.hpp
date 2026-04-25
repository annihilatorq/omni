#pragma once

#include <cstdint>

#include "omni/detail/config.hpp"

#if defined(OMNI_COMPILER_MSVC_COMPAT)
#  include <intrin.h>
#endif

#include "omni/address.hpp"
#include "omni/win/api_set_map.hpp"
#include "omni/win/unicode_string.hpp"

namespace omni::win {

  struct list_entry {
    list_entry* forward_link;
    list_entry* backward_link;
  };

  struct user_process_parameters {
    std::uint8_t reserved1[16];
    void* reserved2[10];
    unicode_string image_path_name;
    unicode_string command_line;
  };

  struct loader_table_entry {
    list_entry in_load_order_links;
    list_entry in_memory_order_links;
    union {
      list_entry in_initialization_order_links;
      list_entry in_progress_links;
    };
    omni::address base_address;
    omni::address entry_point;
    std::uint32_t size_image;
    unicode_string path;
    unicode_string name;
    union {
      std::uint8_t flag_group[4];
      std::uint32_t flags;
      struct {
        std::uint32_t packaged_binary:1;
        std::uint32_t marked_for_removal:1;
        std::uint32_t image_module:1;
        std::uint32_t load_notifications_sent:1;
        std::uint32_t telemetry_entry_processed:1;
        std::uint32_t static_import_processed:1;
        std::uint32_t in_legacy_lists:1;
        std::uint32_t in_indexes:1;
        std::uint32_t shim_module:1;
        std::uint32_t in_exception_table:1;
        std::uint32_t reserved_flags_1:2;
        std::uint32_t load_in_progress:1;
        std::uint32_t load_config_processed:1;
        std::uint32_t entry_point_processed:1;
        std::uint32_t delay_load_protection_enabled:1;
        std::uint32_t reserved_flags_3:2;
        std::uint32_t skip_thread_calls:1;
        std::uint32_t process_attach_called:1;
        std::uint32_t process_attach_failed:1;
        std::uint32_t cor_validation_deferred:1;
        std::uint32_t is_cor_image:1;
        std::uint32_t skip_relocation:1;
        std::uint32_t is_cor_il_only:1;
        std::uint32_t is_chpe_image:1;
        std::uint32_t reserved_flags_5:2;
        std::uint32_t redirected:1;
        std::uint32_t reserved_flags_6:2;
        std::uint32_t compatibility_database_processed:1;
      };
    };
    std::uint16_t obsolete_load_count;
    std::uint16_t tls_index;
    list_entry hash_links;
    std::uint32_t time_date_stamp;
  };

  struct module_loader_data {
    std::uint32_t length;
    std::uint8_t initialized;
    void* ss_handle;
    list_entry in_load_order_module_list;
    list_entry in_memory_order_module_list;
    list_entry in_initialization_order_module_list;
  };

  struct PEB {
    std::uint8_t reserved1[2];
    std::uint8_t being_debugged;
    std::uint8_t reserved2[1];
    void* reserved3[2];
    module_loader_data* loader_data;
    user_process_parameters* process_parameters;
    void* reserved4[3];
    void* atl_thunk_list_head;
    void* reserved5;
    std::uint32_t reserved6;
    void* reserved7;
    std::uint32_t reserved8;
    std::uint32_t atl_thunk_list_head32;
    win::api_set_namespace* api_set_map;
    void* reserved9[44];
    std::uint8_t reserved10[96];

    [[nodiscard]] static auto ptr() {
#if defined(OMNI_ARCH_X64)
#  if defined(OMNI_COMPILER_MSVC_COMPAT)
      return reinterpret_cast<const PEB*>(__readgsqword(0x60));
#  else
      std::uintptr_t address{};
      __asm__ __volatile__("movq %%gs:0x60, %0" : "=r"(address));
      return reinterpret_cast<const PEB*>(address);
#  endif
#elif defined(OMNI_ARCH_X86)
#  if defined(OMNI_COMPILER_MSVC_COMPAT)
      return reinterpret_cast<const PEB*>(__readfsdword(0x30));
#  else
      std::uintptr_t address{};
      __asm__ __volatile__("movl %%fs:0x30, %0" : "=r"(address));
      return reinterpret_cast<const PEB*>(address);
#  endif
#else
#  error Unsupported platform.
#endif
    }
  };

  template <typename T, typename FieldT>
  [[nodiscard]] constexpr T* export_containing_record(FieldT* address, FieldT T::* field) {
    auto offset = reinterpret_cast<std::uintptr_t>(&(reinterpret_cast<T*>(0)->*field));
    return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(address) - offset);
  }

} // namespace omni::win
