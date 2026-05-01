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

#if !defined(OMNI_DISABLE_EXCEPTIONS)
#  if defined(__clang__)
#    if __has_feature(cxx_exceptions)
#      define OMNI_HAS_EXCEPTIONS
#    endif
#  elif defined(_MSC_VER)
#    if defined(_CPPUNWIND)
#      define OMNI_HAS_EXCEPTIONS
#    endif
#  elif defined(__GNUC__)
#    if defined(__EXCEPTIONS)
#      define OMNI_HAS_EXCEPTIONS
#    endif
#  elif defined(__cpp_exceptions)
#    define OMNI_HAS_EXCEPTIONS
#  endif
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

#if !defined(OMNI_HAS_INLINE_SYSCALL)
#  if defined(OMNI_ARCH_X64) && (defined(OMNI_COMPILER_CLANG) || defined(OMNI_COMPILER_GCC))
#    define OMNI_HAS_INLINE_SYSCALL
#  endif
#endif

#if defined(OMNI_COMPILER_MSVC_COMPAT)
#  define OMNI_FORCEINLINE __forceinline
#elif defined(OMNI_COMPILER_CLANG) || defined(OMNI_COMPILER_GCC)
#  define OMNI_FORCEINLINE inline __attribute__((__always_inline__))
#else
#  define OMNI_FORCEINLINE inline
#endif

#if __has_cpp_attribute(msvc::no_unique_address)
#  define OMNI_NO_UNIQUE_ADDRESS [[msvc::no_unique_address]]
#elif __has_cpp_attribute(no_unique_address)
#  define OMNI_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#  define OMNI_NO_UNIQUE_ADDRESS
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
