#ifndef SHADOW_MEMORY_CHECKSUM_HPP
#define SHADOW_MEMORY_CHECKSUM_HPP

#include <concepts>
#include <limits>
#include <numeric>
#include <span>

namespace shadow {
    // \note: Some useful benchmarks to understand the
    // difference in the bytewise vs collection rate
    // using SSE intrinsics, benched using 2MB span:
    // [MSVC]
    // BM_SumBytesBasic     792456 ns       802176 ns
    // BM_SumBytesSSE        85171 ns        85794 ns
    // [CLANG]
    // BM_SumBytesBasic     381638 ns       383650 ns
    // BM_SumBytesSSE        98902 ns        97656 ns

    template <std::integral Ty = std::size_t>
    class memory_checksum {
        using vector128_t = __m128i;

    public:
        explicit memory_checksum( const std::span<const char> data ) noexcept {
            const auto size = data.size();
            auto sum = _mm_setzero_si128();
            std::size_t pos = 0;

            // The main feature of vectorized byte collection is
            // that we do not iterate each byte separately, but
            // load 16 bytes in one iteration, respectively, the
            // number of iterations is reduced by 16 times.

            for ( ; pos + 16 <= size; pos += 16 )
                process_block( data, pos, sum );

            // Just sum up all 16-bit words from the "sum"
            m_result = sum_16bit_words( sum );

            // If the sum of bytes is not a multiple of 16, there
            // will be a "tail" of remaining bytes, collect them.
            m_result += append_tail( data, pos ) * std::numeric_limits<Ty>::max();
        }

        [[nodiscard]] Ty result() const noexcept {
            return m_result;
        }

    private:
        void process_block( const std::span<const char> data, std::size_t pos, vector128_t& sum ) const {
            // Load 16 bytes in one-go
            auto block = _mm_loadu_si128( reinterpret_cast<const vector128_t*>( &data[pos] ) );

            // We will not use '_mm_cvtepi8_epi16' in order
            // not to switch from SSE2 to SSE 4.1
            auto low_eight_bytes = _mm_unpacklo_epi8( block, _mm_setzero_si128() );
            auto high_eight_bytes = _mm_unpackhi_epi8( block, _mm_setzero_si128() );

            sum = _mm_add_epi16( sum, low_eight_bytes );
            sum = _mm_add_epi16( sum, high_eight_bytes );
        }

        Ty sum_16bit_words( const vector128_t& sum ) const {
            alignas( 16 ) int16_t temp[8];
            _mm_storeu_si128( reinterpret_cast<vector128_t*>( temp ), sum );

            return std::accumulate( std::begin( temp ), std::end( temp ), Ty{ 0 } );
        }

        Ty append_tail( const std::span<const char> data, std::size_t pos ) const {
            return std::reduce( data.begin() + pos, data.end(), 0ull, []( Ty acc, char byte ) {
                return acc + static_cast<unsigned char>( byte );
            } );
        }

        Ty m_result{ 0 };
    };
} // namespace shadow

#endif // SHADOW_MEMORY_CHECKSUM_HPP