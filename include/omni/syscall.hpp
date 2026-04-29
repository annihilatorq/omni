#pragma once

#include <expected>
#include <system_error>
#include <utility>

#include "omni/address.hpp"
#include "omni/concepts/concepts.hpp"
#include "omni/detail/config.hpp"
#include "omni/detail/extract_function_name.hpp"
#include "omni/detail/inplace_function.hpp"
#include "omni/detail/memory_cache.hpp"
#include "omni/detail/normalize_pointer_argument.hpp"
#include "omni/detail/shellcode.hpp"
#include "omni/error.hpp"
#include "omni/hash.hpp"
#include "omni/module_export.hpp"
#include "omni/status.hpp"

namespace omni {

  namespace detail {
#ifdef OMNI_HAS_CACHING
    // Use std::uint64_t as a key to store underlying value type of any hash
    inline detail::memory_cache<std::uint64_t, std::uint32_t> syscall_id_cache;
#endif
  } // namespace detail

  // Syscall ID is at an offset of 4 bytes from the specified address.
  // Not considering the situation when EDR hook is installed
  // Learn more here: https://github.com/annihilatorq/shadow_syscall/issues/1
  inline std::expected<std::uint32_t, std::error_code> default_syscall_id_parser(const omni::named_export& module_export) {
    auto* address = module_export.address.ptr<std::uint8_t>();

    for (std::size_t i{}; i < 24; ++i) {
      if (address[i] == 0x4c && address[i + 1] == 0x8b && address[i + 2] == 0xd1 && address[i + 3] == 0xb8 &&
          address[i + 6] == 0x00 && address[i + 7] == 0x00) {
        return *reinterpret_cast<std::uint32_t*>(&address[i + 4]);
      }
    }

    return std::unexpected(omni::error::syscall_id_not_found);
  }

  template <typename T = omni::status>
    requires(omni::detail::is_x64)
  class syscaller {
   public:
    using syscall_id_parser = detail::inplace_function<std::expected<std::uint32_t, std::error_code>(omni::named_export)>;

    explicit syscaller(concepts::hash auto export_name, syscall_id_parser parser = default_syscall_id_parser)
      : syscall_id_parser_(std::move(parser)), syscall_id_(resolve_syscall_id(export_name)) {
      if (syscall_id_) {
        setup_shellcode(*syscall_id_);
      }
    }

    explicit syscaller(default_hash export_name, syscall_id_parser parser = default_syscall_id_parser)
      : syscall_id_parser_(std::move(parser)), syscall_id_(resolve_syscall_id(export_name)) {
      if (syscall_id_) {
        setup_shellcode(*syscall_id_);
      }
    }

    template <typename... Args>
    std::expected<T, std::error_code> try_invoke(Args&&... args) {
      if (!syscall_id_) {
        return std::unexpected(syscall_id_.error());
      }
      if constexpr (std::is_void_v<T>) {
        shellcode_.execute(detail::normalize_pointer_argument(args)...);
        return {};
      } else {
        return shellcode_.execute<T>(detail::normalize_pointer_argument(args)...);
      }
    }

    template <typename... Args>
    T invoke(Args&&... args) {
      if constexpr (std::is_void_v<T>) {
        std::ignore = try_invoke(std::forward<Args>(args)...);
      } else {
        return try_invoke(std::forward<Args>(args)...).value_or(T{});
      }
    }

    template <typename... Args>
    T operator()(Args&&... args) {
      return invoke(std::forward<Args>(args)...);
    }

   private:
    void setup_shellcode(std::uint32_t syscall_id) {
      shellcode_.write<std::uint32_t>(6, syscall_id);
      shellcode_.setup();
    }

    std::expected<std::uint32_t, std::error_code> resolve_syscall_id(concepts::hash auto export_name) {
#ifdef OMNI_HAS_CACHING
      auto cached_syscall_id = detail::syscall_id_cache.try_get(export_name.value());
      if (cached_syscall_id.has_value()) {
        return cached_syscall_id.value();
      }
#endif
      omni::named_export named_export = omni::get_export(export_name);
      if (!named_export.present()) {
        return std::unexpected(omni::error::export_not_found);
      }

      auto parsed_syscall_id = syscall_id_parser_(named_export);
      if (!parsed_syscall_id) {
        return std::unexpected(parsed_syscall_id.error());
      }

#ifdef OMNI_HAS_CACHING
      detail::syscall_id_cache.set(export_name.value(), *parsed_syscall_id);
#endif

      return *parsed_syscall_id;
    }

    syscall_id_parser syscall_id_parser_;
    std::expected<std::uint32_t, std::error_code> syscall_id_;

    detail::shellcode<13> shellcode_{{0x49, 0x89, 0xCA, 0x48, 0xC7, 0xC0, 0x3F, 0x10, 0x00, 0x00, 0x0F, 0x05, 0xC3}};
  };

  template <typename T, typename... Params>
    requires(omni::detail::is_x64)
  class syscaller<T (*)(Params...)> : public syscaller<T> {
   public:
    using syscaller<T>::syscaller;

    std::expected<T, std::error_code> try_invoke(Params... args) {
      return syscaller<T>::try_invoke(args...);
    }

    T invoke(Params... args) {
      return syscaller<T>::invoke(args...);
    }

    T operator()(Params... args) {
      return syscaller<T>::invoke(args...);
    }
  };

  template <typename T = omni::status, concepts::hash Hasher, typename... Args>
    requires(!concepts::function_pointer<T>)
  inline T syscall(Hasher export_name, Args&&... args) {
    return syscaller<T>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <typename T = omni::status, typename... Args>
    requires(!concepts::function_pointer<T>)
  inline T syscall(default_hash export_name, Args&&... args) {
    return syscall<T, default_hash>(export_name, std::forward<Args>(args)...);
  }

  template <auto Func, concepts::hash Hasher, class... Args>
  inline auto syscall(Args&&... args) {
    constexpr Hasher func_name{detail::extract_function_name<Func>()};
    return syscaller<decltype(Func)>{func_name}.invoke(std::forward<Args>(args)...);
  }

  template <auto Func, class... Args>
  inline auto syscall(Args&&... args) {
    return syscall<Func, default_hash>(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, concepts::hash Hasher, class... Args>
  inline auto syscall(Hasher export_name, Args&&... args) {
    return syscaller<F>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, class... Args>
  inline auto syscall(default_hash export_name, Args&&... args) {
    return syscall<F, default_hash>(export_name, std::forward<Args>(args)...);
  }

} // namespace omni
