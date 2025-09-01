#ifndef SHADOW_WIN_PEB_HPP
#define SHADOW_WIN_PEB_HPP

#include "list_entry.hpp"
#include "unicode_string.hpp"
#include <Windows.h>

namespace win {
    struct module_loader_data {
        std::uint32_t length;
        std::uint8_t initialized;
        void* ss_handle;
        list_entry in_load_order_module_list;
        list_entry in_memory_order_module_list;
        list_entry in_initialization_order_module_list;
    };

    struct user_process_parameters {
        uint8_t reserved1[16];
        std::nullptr_t reserved2[10];
        unicode_string image_path_name;
        unicode_string command_line;
    };

    struct PEB {
        uint8_t reserved1[2];
        uint8_t being_debugged;
        uint8_t reserved2[1];
        std::nullptr_t reserved3[2];
        module_loader_data* ldr_data;
        user_process_parameters* process_parameters;
        std::nullptr_t reserved4[3];
        void* atl_thunk_list_head;
        std::nullptr_t reserved5;
        uint32_t reserved6;
        std::nullptr_t reserved7;
        uint32_t reserved8;
        uint32_t atl_thunk_list_head32;
        void* reserved9[45];
        uint8_t reserved10[96];

        static auto address() noexcept {
#if defined( _M_X64 )
            return reinterpret_cast<const PEB*>( __readgsqword( 0x60 ) );
#elif defined( _M_IX86 )
            return reinterpret_cast<const PEB*>( __readfsdword( 0x30 ) );
#else
    #error Unsupported platform.
#endif
        }

        static auto loader_data() noexcept {
            return reinterpret_cast<module_loader_data*>( address()->ldr_data );
        }
    };
} // namespace win

#endif // SHADOW_WIN_PEB_HPP
