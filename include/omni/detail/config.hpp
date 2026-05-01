#pragma once

#if defined(__clang__)
#  define OMNI_COMPILER_CLANG
#  if defined(_MSC_VER)
#    define OMNI_COMPILER_MSVC_COMPAT
#  endif
#elif defined(_MSC_VER)
#  define OMNI_COMPILER_MSVC
#  define OMNI_COMPILER_MSVC_COMPAT
#elif defined(__GNUC__)
#  define OMNI_COMPILER_GCC
#endif

#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
#  define OMNI_ARCH_X64
#elif defined(_M_IX86) || defined(__i386__)
#  define OMNI_ARCH_X86
#endif

#if !defined(OMNI_DISABLE_EXCEPTIONS) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
#  define OMNI_HAS_EXCEPTIONS
#endif

#if !defined(OMNI_HAS_CACHING)
#  if !defined(OMNI_DISABLE_CACHING)
#    define OMNI_HAS_CACHING
#  endif
#endif

#if !defined(OMNI_DISABLE_ERROR_STRINGS)
#  if defined(OMNI_ENABLE_ERROR_STRINGS) || defined(DEBUG) || defined(_DEBUG)
#    define OMNI_HAS_ERROR_STRINGS
#  endif
#endif

#if defined(_MSC_VER)
#  define OMNI_FORCEINLINE __forceinline
#else
#  define OMNI_FORCEINLINE __attribute__((always_inline))
#endif

namespace omni::detail {
#ifdef OMNI_ARCH_X64
  [[maybe_unused]] constexpr inline bool is_x64 = true;
#else
  [[maybe_unused]] constexpr inline bool is_x64 = false;
#endif

#ifdef OMNI_ARCH_X86
  [[maybe_unused]] constexpr inline bool is_x86 = true;
#else
  [[maybe_unused]] constexpr inline bool is_x86 = false;
#endif
} // namespace omni::detail
