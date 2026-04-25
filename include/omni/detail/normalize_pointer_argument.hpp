#pragma once

#include <concepts>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <utility>

namespace omni::detail {

  template <typename T>
  inline auto normalize_pointer_argument(T&& arg) {
    using value_type = std::remove_cvref_t<T>;

    if constexpr (std::is_array_v<value_type>) {
      return std::data(arg);
    } else {
      // All credits to @Debounce, huge thanks to him/her!
      //
      // Since arguments after the fourth are written on the stack,
      // the compiler will fill the lower 32 bits from int with null,
      // and the upper 32 bits will remain undefined.
      //
      // Because the syscall handler expects a (void*)-sized pointer
      // there, this address will be garbage for it, hence AV.
      // If the argument went 1/2/3/4, the compiler would generate a
      // write to ecx/edx/r8d/r9d, by x64 convention, writing to the
      // lower half of a 64-bit register zeroes the upper part too
      // (i.e. ecx = 0 => rcx = 0), so this problem should only exist
      // on x64 for arguments after the fourth.
      // The solution would be on templates to loop through all
      // arguments and manually cast them to size_t size.

      constexpr auto is_signed_integral = std::signed_integral<value_type>;
      constexpr auto is_unsigned_integral = std::unsigned_integral<value_type>;

      using unsigned_integral_type = std::conditional_t<is_unsigned_integral, std::uintptr_t, value_type>;
      using tag_type = std::conditional_t<is_signed_integral, std::intptr_t, unsigned_integral_type>;

      return static_cast<tag_type>(std::forward<T>(arg));
    }
  }

} // namespace omni::detail
