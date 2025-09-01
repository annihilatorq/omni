#ifndef SHADOW_ADDRESS_HPP
#define SHADOW_ADDRESS_HPP

#include "concepts.hpp"
#include <ostream>
#include <span>

namespace shadow {
    class address_t {
    public:
        using underlying_t = std::uintptr_t;

        constexpr address_t() = default;
        // this way, we don't get the `ambiguous_condition` error
        constexpr address_t( concepts::nullpointer auto ) noexcept { }
        constexpr address_t( underlying_t address ) noexcept: m_address( address ) { }
        constexpr address_t( concepts::pointer auto address ) noexcept: m_address( reinterpret_cast<underlying_t>( address ) ) { }
        constexpr address_t( std::ranges::contiguous_range auto range ) noexcept: m_address( reinterpret_cast<underlying_t>( range.data() ) ) { }

        address_t( const address_t& ) = default;
        address_t( address_t&& ) = default;
        address_t& operator=( const address_t& ) = default;
        address_t& operator=( address_t&& ) = default;
        ~address_t() = default;

        template <typename Ty = void, typename PointerTy = std::add_pointer_t<Ty>>
        [[nodiscard]] constexpr PointerTy ptr( std::ptrdiff_t offset = 0 ) const noexcept {
            return this->offset( offset ).as<PointerTy>();
        }

        [[nodiscard]] constexpr underlying_t get() const noexcept {
            return m_address;
        }

        template <typename Ty = address_t>
        [[nodiscard]] constexpr Ty offset( std::ptrdiff_t offset = 0 ) const noexcept {
            if constexpr ( std::is_pointer_v<Ty> )
                return m_address == 0u ? nullptr : reinterpret_cast<Ty>( m_address + offset );
            else
                return m_address == 0u ? static_cast<Ty>( *this ) : Ty{ m_address + offset };
        }

        template <typename Ty>
        [[nodiscard]] constexpr Ty as() const noexcept {
            if constexpr ( std::is_pointer_v<Ty> )
                return reinterpret_cast<Ty>( m_address );
            else
                return static_cast<Ty>( m_address );
        }

        template <typename Ty>
        [[nodiscard]] constexpr std::span<Ty> span( std::size_t count ) const noexcept {
            return { this->ptr<Ty>(), count };
        }

        [[nodiscard]] bool is_in_range( const address_t& start, const address_t& end ) const noexcept {
            return ( *this >= start ) && ( *this < end );
        }

        template <typename Ty, typename... Args>
        [[nodiscard]] Ty execute( Args&&... args ) const noexcept {
            if ( m_address == 0 ) {
                if constexpr ( std::is_pointer_v<Ty> )
                    return nullptr;
                else
                    return Ty{};
            }

            using target_function_t = Ty( __stdcall* )( std::decay_t<Args>... );
            const auto target_function = reinterpret_cast<target_function_t>( m_address );

            return target_function( std::forward<Args>( args )... );
        }

        constexpr explicit operator std::uintptr_t() const noexcept {
            return m_address;
        }
        constexpr explicit operator bool() const noexcept {
            return static_cast<bool>( m_address );
        }
        constexpr auto operator<=>( const address_t& ) const = default;

        constexpr address_t operator+=( const address_t& rhs ) noexcept {
            m_address += rhs.m_address;
            return *this;
        }

        constexpr address_t operator-=( const address_t& rhs ) noexcept {
            m_address -= rhs.m_address;
            return *this;
        }

        [[nodiscard]] constexpr address_t operator+( const address_t& rhs ) const noexcept {
            return { m_address + rhs.m_address };
        }

        [[nodiscard]] constexpr address_t operator-( const address_t& rhs ) const noexcept {
            return { m_address - rhs.m_address };
        }

        [[nodiscard]] constexpr address_t operator&( const address_t& other ) const noexcept {
            return { m_address & other.m_address };
        }

        [[nodiscard]] constexpr address_t operator|( const address_t& other ) const noexcept {
            return { m_address | other.m_address };
        }

        [[nodiscard]] constexpr address_t operator^( const address_t& other ) const noexcept {
            return { m_address ^ other.m_address };
        }

        [[nodiscard]] constexpr address_t operator<<( std::size_t shift ) const noexcept {
            return { m_address << shift };
        }

        [[nodiscard]] constexpr address_t operator>>( std::size_t shift ) const noexcept {
            return { m_address >> shift };
        }

        friend std::ostream& operator<<( std::ostream& os, const address_t& address ) {
            return os << address.ptr();
        }

    private:
        underlying_t m_address{ 0 };
    };
} // namespace shadow

#endif // SHADOW_ADDRESS_HPP
