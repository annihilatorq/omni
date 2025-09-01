#ifndef SHADOW_CONCEPTS_HPP
#define SHADOW_CONCEPTS_HPP

#include <chrono>

namespace shadow::concepts {

    template <typename Ty> concept arithmetic = std::is_arithmetic_v<Ty>;
    template <typename Ty> concept pointer = std::is_pointer_v<Ty>;
    template <typename Ty> concept nullpointer = std::is_null_pointer_v<Ty>;
    template <typename Ty>
    concept indexable_sizeable = requires( Ty t ) {
        { t.size() } -> std::convertible_to<std::size_t>;
        { t[0] } -> std::convertible_to<typename Ty::value_type>;
    };
    template <typename Ty> concept hashable = indexable_sizeable<Ty>;
    template <typename Ty> concept chrono_duration = std::is_base_of_v<std::chrono::duration<typename Ty::rep, typename Ty::period>, Ty>;

    template <typename Ty>
    concept fundamental = [] {
        static_assert( std::is_fundamental_v<Ty>, "Nt/Zw functions cannot return the type you specified."
                                                  "Type should be fundamental." );
        return true;
    }();

    template <typename Ty>
    using non_void_t = std::conditional_t<std::is_void_v<Ty>, std::monostate, Ty>;

    template <typename Ty>
    constexpr bool is_type_ntstatus = std::is_same_v<std::remove_cv_t<Ty>, long>;

} // namespace shadow::concepts

#endif // SHADOW_CONCEPTS_HPP
