#ifndef SHADOW_WIN_LOADER_TABLE_ENTRY_HPP
#define SHADOW_WIN_LOADER_TABLE_ENTRY_HPP

#include "../address.hpp"
#include "list_entry.hpp"
#include "unicode_string.hpp"

namespace win {
    struct loader_table_entry {
        list_entry in_load_order_links;
        list_entry in_memory_order_links;
        union {
            list_entry in_initialization_order_links;
            list_entry in_progress_links;
        };
        shadow::address_t base_address;
        shadow::address_t entry_point;
        std::uint32_t size_image;
        unicode_string path;
        unicode_string name;
        union {
            std::uint8_t flag_group[4];
            std::uint32_t flags;
            struct {
                std::uint32_t packaged_binary:1;
                std::uint32_t marked_for_removal:1;
                std::uint32_t image_dll:1;
                std::uint32_t load_notifications_sent:1;
                std::uint32_t telemetry_entry_processed:1;
                std::uint32_t static_import_processed:1;
                std::uint32_t in_legacy_lists:1;
                std::uint32_t in_indexes:1;
                std::uint32_t shim_dll:1;
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

    template <typename T, typename FieldT>
    constexpr T* containing_record( FieldT* address, FieldT T::* field ) {
        auto offset = reinterpret_cast<std::uintptr_t>( &( reinterpret_cast<T*>( 0 )->*field ) );
        return reinterpret_cast<T*>( reinterpret_cast<std::uintptr_t>( address ) - offset );
    }
} // namespace win

#endif // SHADOW_WIN_LOADER_TABLE_ENTRY_HPP
