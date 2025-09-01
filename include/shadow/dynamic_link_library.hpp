#ifndef SHADOW_DYNAMIC_LINK_LIBRARY_HPP
#define SHADOW_DYNAMIC_LINK_LIBRARY_HPP

#include "hash.hpp"
#include "memory_checksum.hpp"
#include "win/loader_table_entry.hpp"
#include <nt/image.hpp>

namespace shadow {
    class dynamic_link_library {
    public:
        constexpr dynamic_link_library() noexcept = default;
        dynamic_link_library( hash64_t module_name ): m_data( find( module_name ).loader_table_entry() ) { }
        dynamic_link_library( win::loader_table_entry* module_data ): m_data( module_data ) { }

        dynamic_link_library( const dynamic_link_library& instance ) = default;
        dynamic_link_library( dynamic_link_library&& instance ) = default;
        dynamic_link_library& operator=( const dynamic_link_library& instance ) = default;
        dynamic_link_library& operator=( dynamic_link_library&& instance ) = default;
        ~dynamic_link_library() = default;

        // \return loader_table_entry* - Raw pointer to Win32
        // loader data about the current module
        [[nodiscard]] win::loader_table_entry* loader_table_entry() const noexcept {
            return m_data;
        }

        // \return image_t - Displaying an image in process
        // memory using the image_t structure from "linuxpe"
        [[nodiscard]] auto image() const noexcept {
            return m_data->base_address.ptr<win::image_x64_t>();
        }

        // \return uint16_t - How many references the current
        // DLL has at the moment
        [[nodiscard]] auto reference_count() const noexcept {
            return loader_table_entry()->obsolete_load_count;
        }

        // \return address_t - Base address of current DLL
        [[nodiscard]] auto base_address() const noexcept {
            return m_data->base_address;
        }

        // \return void* - Pointer on base address of current
        // DLL, same as GetModuleHandle() in Win32 API
        [[nodiscard]] auto native_handle() const noexcept {
            return base_address().ptr();
        }

        // \return Address of entrypoint
        [[nodiscard]] auto entry_point() const noexcept {
            return m_data->entry_point;
        }

        // \return Name of current DLL as std::wstring_view
        [[nodiscard]] auto name() const noexcept {
            return m_data == nullptr ? win::unicode_string{} : m_data->name;
        }

        // \return Filepath to current DLL as std::wstring_view
        [[nodiscard]] auto filepath() const noexcept {
            return m_data == nullptr ? win::unicode_string{} : m_data->path;
        }

        // \return Exports range-enumerator of current DLL
        /*[[nodiscard]] auto exports() const noexcept {
          return export_view{m_data->base_address};
        }*/

        template <std::integral Ty = std::size_t>
        [[nodiscard]] auto section_checksum( hash32_t section_name = ".text" ) const {
            const auto module_base = base_address();
            const auto sections = image()->get_nt_headers()->sections();
            auto section = std::find_if( sections.begin(), sections.end(), [=]( const win::section_header_t& section ) {
                return section_name == hash32_t{}( section.name.to_string() );
            } );

            const auto section_content = std::span{ module_base.ptr<uint8_t>( section->virtual_address ), section->virtual_size };
            return memory_checksum<Ty>{ section_content }.result();
        }

        [[nodiscard]] auto present() const noexcept {
            return m_data != nullptr;
        }

        [[nodiscard]] bool operator==( const dynamic_link_library& other ) const noexcept {
            return m_data == other.m_data;
        }

        [[nodiscard]] bool operator==( hash64_t module_name_hash ) const noexcept {
            const auto module_name = name().view();
            // Try to compare hash of full module name
            const auto full_name_hash = hash64_t{}( module_name );

            if ( module_name.size() <= 4 )
                return module_name_hash == full_name_hash;

            // Try to compare hash of trimmed module name (.dll)
            const auto trimmed_name = module_name.substr( 0, module_name.size() - 4 );
            const auto trimmed_name_hash = hash64_t{}( trimmed_name );

            // Verify both hashes
            return full_name_hash == module_name_hash || trimmed_name_hash == module_name_hash;
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return present();
        }

        friend std::ostream& operator<<( std::ostream& os, const dynamic_link_library& dll ) {
            return os << dll.name();
        }

    private:
        dynamic_link_library find( hash64_t module_name ) const;
        win::loader_table_entry* m_data{ nullptr };
    };
} // namespace shadow

#endif // SHADOW_DYNAMIC_LINK_LIBRARY_HPP
