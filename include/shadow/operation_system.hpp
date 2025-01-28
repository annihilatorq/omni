#ifndef SHADOW_OPERATION_SYSTEM_HPP
#define SHADOW_OPERATION_SYSTEM_HPP

#include <cstdint>
#include <format>

namespace shadow {
    class operation_system {
    public:
        constexpr operation_system( std::uint32_t major, std::uint32_t minor, std::uint32_t build_num ) noexcept
            : m_major_version( major ), m_minor_version( minor ), m_build_number( build_num ) { }

        [[nodiscard]] auto is_windows_11() const {
            return m_major_version == 10 && m_build_number >= 22000;
        }

        [[nodiscard]] auto is_windows_10() const {
            return m_major_version == 10 && m_build_number < 22000;
        }

        [[nodiscard]] auto is_windows_8_1() const {
            return verify_version_mask( 6, 3 );
        }

        [[nodiscard]] auto is_windows_8() const {
            return verify_version_mask( 6, 2 );
        }

        [[nodiscard]] auto is_windows_7() const {
            return verify_version_mask( 6, 1 );
        }

        [[nodiscard]] auto is_windows_xp() const {
            return verify_version_mask( 6, 0 );
        }

        [[nodiscard]] auto is_windows_vista() const {
            return verify_version_mask( 5, 1 );
        }

        [[nodiscard]] auto major_version() const {
            return m_major_version;
        }

        [[nodiscard]] auto minor_version() const {
            return m_minor_version;
        }

        [[nodiscard]] auto build_number() const {
            return m_build_number;
        }

        [[nodiscard]] auto formatted() const {
            return std::format( "Windows {}.{} (Build {})", m_major_version, m_minor_version, m_build_number );
        }

    private:
        bool verify_version_mask( std::uint32_t major, std::uint32_t minor ) const {
            return m_major_version == major && m_minor_version == minor;
        }

        std::uint32_t m_major_version, m_minor_version, m_build_number;
    };
} // namespace shadow

#endif // SHADOW_OPERATION_SYSTEM_HPP