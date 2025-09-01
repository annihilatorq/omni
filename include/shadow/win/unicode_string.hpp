#ifndef SHADOW_WIN_UNICODE_STRING_HPP
#define SHADOW_WIN_UNICODE_STRING_HPP

#include <cstdint>
#include <filesystem>
#include <ranges>
#include <string_view>

namespace win {
    struct unicode_string {
        using char_t = wchar_t;
        using pointer_t = char_t*;

    public:
        constexpr unicode_string() = default;
        constexpr unicode_string( pointer_t buffer, std::uint16_t length, std::uint16_t max_length = 0 ) noexcept
            : m_length( length ), m_max_length( max_length ), m_buffer( buffer ) { }

        unicode_string( const unicode_string& instance ) = default;
        unicode_string( unicode_string&& instance ) = default;
        unicode_string& operator=( const unicode_string& instance ) = default;
        unicode_string& operator=( unicode_string&& instance ) = default;
        ~unicode_string() = default;

        template <typename Ty>
            requires( std::is_constructible_v<Ty, pointer_t> )
        [[nodiscard]] auto as() const noexcept( std::is_nothrow_constructible_v<Ty, pointer_t> ) {
            return Ty{ m_buffer };
        }

        [[nodiscard]] auto to_path( std::filesystem::path::format fmt = std::filesystem::path::auto_format ) const {
            return std::filesystem::path{ m_buffer, fmt };
        }

        [[nodiscard]] auto view() const noexcept {
            return std::wstring_view{ m_buffer, m_length / sizeof( char_t ) };
        }

        [[nodiscard]] auto string() const {
            // \note: Since std::codecvt & std::wstring_convert is
            // deprecated in cpp17 and will be deleted in
            // cpp26, we use the std::filesystem::path
            // as a string converter, although it will
            // require more memory, we will be sure
            // that the conversion will be correct.
            // We will not use wcstombs_s because of the
            // dependency on the current locale.

            const auto source_str = view();
            const auto has_non_ascii_symbols = contains_non_ascii( source_str );
            if ( has_non_ascii_symbols ) {
                // Use std::filesystem::path as string converter
                return to_path().string();
            } else {
                // Otherwise, return string_view converted to std::string
                return std::string( source_str.begin(), source_str.end() );
            }
        }

        [[nodiscard]] auto data() const noexcept {
            return m_buffer;
        }
        [[nodiscard]] auto size() const noexcept {
            return m_length;
        }
        [[nodiscard]] auto empty() const noexcept {
            return m_buffer == nullptr || m_length == 0;
        }

        [[nodiscard]] bool operator==( const unicode_string& right ) const noexcept {
            return m_buffer == right.m_buffer && m_length == right.m_length;
        }

        [[nodiscard]] bool operator==( std::wstring_view right ) const noexcept {
            return view() == right;
        }

        [[nodiscard]] bool operator==( std::string_view right ) const noexcept {
            const auto src_view = view();
            if ( src_view.size() != right.size() )
                return false;

            auto wide_string_transformed_to_ascii = std::ranges::transform_view( src_view, []( wchar_t wc ) -> char {
                return static_cast<char>( wc );
            } );

            return std::ranges::equal( wide_string_transformed_to_ascii, right );
        }

        [[nodiscard]] explicit operator bool() const noexcept {
            return m_buffer != nullptr;
        }

        friend std::ostream& operator<<( std::ostream& os, const unicode_string& unicode_str ) {
            return os << unicode_str.string();
        }

    private:
        [[nodiscard]] bool contains_non_ascii( const std::wstring_view str ) const noexcept {
            return std::ranges::any_of( str, []( wchar_t ch ) {
                return ch > 127; // characters out of ASCII range
            } );
        }

        std::uint16_t m_length{ 0 };
        std::uint16_t m_max_length{ 0 };
        pointer_t m_buffer{ nullptr };
    };
} // namespace win

#endif // SHADOW_WIN_UNICODE_STRING_HPP
