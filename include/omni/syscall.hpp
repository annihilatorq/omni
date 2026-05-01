#pragma once

#include <expected>
#include <system_error>
#include <type_traits>
#include <utility>

#include "omni/address.hpp"
#include "omni/concepts/concepts.hpp"
#include "omni/detail/config.hpp"
#include "omni/detail/extract_function_name.hpp"
#include "omni/detail/memory_cache.hpp"
#include "omni/detail/normalize_pointer_argument.hpp"
#include "omni/detail/shellcode.hpp"
#include "omni/error.hpp"
#include "omni/hash.hpp"
#include "omni/module_export.hpp"
#include "omni/status.hpp"

#ifdef OMNI_HAS_INLINE_SYSCALL
#  include "omni/detail/inline_syscall.hpp"
#endif

namespace omni {

  namespace concepts {
    template <typename Parser>
    concept syscall_id_parser =
      std::invocable<Parser, const omni::named_export&> &&
      std::same_as<std::invoke_result_t<Parser, const omni::named_export&>, std::expected<std::uint32_t, std::error_code>>;

    template <typename Invoker, typename... Args> concept syscall_invoker = std::invocable<Invoker, std::uint32_t, Args&&...>;
  } // namespace concepts

  namespace detail {
#ifdef OMNI_HAS_CACHING
    // Use std::uint64_t as a key to store underlying value type of any hash
    inline detail::memory_cache<std::uint64_t, std::uint32_t> syscall_id_cache;
#endif
  } // namespace detail

  struct default_syscall_id_parser {
    // Syscall ID is at an offset of 4 bytes from the specified address.
    // Not considering the situation when EDR hook is installed
    // Learn more here: https://github.com/annihilatorq/shadow_syscall/issues/1
    std::expected<std::uint32_t, std::error_code> operator()(const omni::named_export& module_export) const {
      auto* address = module_export.address.ptr<std::uint8_t>();

      for (std::size_t i{}; i < 24; ++i) {
        if (address[i] == 0x4c && address[i + 1] == 0x8b && address[i + 2] == 0xd1 && address[i + 3] == 0xb8 &&
            address[i + 6] == 0x00 && address[i + 7] == 0x00) {
          return *reinterpret_cast<std::uint32_t*>(&address[i + 4]);
        }
      }

      return std::unexpected(omni::error::syscall_id_not_found);
    }
  };

  static_assert(concepts::syscall_id_parser<default_syscall_id_parser>);

  struct shellcode_syscall_invoker {
    bool shellcode_initialized{false};
    detail::shellcode<13> shellcode{{0x49, 0x89, 0xCA, 0x48, 0xC7, 0xC0, 0x3F, 0x10, 0x00, 0x00, 0x0F, 0x05, 0xC3}};

    template <typename T = omni::status, typename... Args>
    T operator()(std::uint32_t syscall_id, Args&&... args) {
      if (!shellcode_initialized) {
        shellcode.write<std::uint32_t>(6, syscall_id);
        shellcode.setup();
        shellcode_initialized = true;
      }

      if constexpr (std::is_void_v<T>) {
        shellcode.execute(std::forward<Args>(args)...);
      } else {
        return shellcode.execute<T>(std::forward<Args>(args)...);
      }
    }
  };

  static_assert(concepts::syscall_invoker<shellcode_syscall_invoker, omni::status>);

#ifdef OMNI_HAS_INLINE_SYSCALL
  struct inline_syscall_invoker {
    template <typename T = omni::status, typename... Args>
    T operator()(std::uint32_t syscall_id, Args&&... args) {
      if constexpr (std::is_void_v<T>) {
        detail::syscall(syscall_id, std::forward<Args>(args)...);
      } else {
        return T{detail::syscall(syscall_id, std::forward<Args>(args)...)};
      }
    }
  };

  static_assert(concepts::syscall_invoker<inline_syscall_invoker, omni::status>);
#endif

  template <concepts::syscall_id_parser Parser = default_syscall_id_parser,
    concepts::syscall_invoker Invoker = shellcode_syscall_invoker>
  struct syscaller_options {
    OMNI_NO_UNIQUE_ADDRESS Parser parser;
    OMNI_NO_UNIQUE_ADDRESS Invoker invoker;
  };

  using default_syscaller_options = syscaller_options<default_syscall_id_parser, shellcode_syscall_invoker>;

#ifdef OMNI_HAS_INLINE_SYSCALL
  using inline_syscaller_options = syscaller_options<default_syscall_id_parser, inline_syscall_invoker>;
#endif

  template <typename T = omni::status, typename Options = default_syscaller_options>
    requires(omni::detail::is_x64)
  class syscaller {
   public:
    explicit syscaller(concepts::hash auto export_name, Options options = {})
      : options_(std::move(options)), syscall_id_(resolve_syscall_id(export_name)) {}

    explicit syscaller(default_hash export_name, Options options = {})
      : options_(std::move(options)), syscall_id_(resolve_syscall_id(export_name)) {}

    template <typename... Args>
    std::expected<T, std::error_code> try_invoke(Args&&... args) {
      if (!syscall_id_) {
        return std::unexpected(syscall_id_.error());
      }
      if constexpr (std::is_void_v<T>) {
        options_.invoker.template operator()<void>(*syscall_id_,
          detail::normalize_pointer_argument(std::forward<Args>(args))...);
        return {};
      } else {
        return options_.invoker.template operator()<T>(*syscall_id_,
          detail::normalize_pointer_argument(std::forward<Args>(args))...);
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

      auto parsed_syscall_id = options_.parser(named_export);
      if (!parsed_syscall_id) {
        return std::unexpected(parsed_syscall_id.error());
      }

#ifdef OMNI_HAS_CACHING
      detail::syscall_id_cache.set(export_name.value(), *parsed_syscall_id);
#endif

      return *parsed_syscall_id;
    }

    OMNI_NO_UNIQUE_ADDRESS Options options_;
    std::expected<std::uint32_t, std::error_code> syscall_id_;
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

  template <typename Result, typename Options, typename... Params>
    requires(omni::detail::is_x64)
  class syscaller<Result (*)(Params...), Options> : public syscaller<Result, Options> {
   public:
    using base_type = syscaller<Result, Options>;
    using base_type::base_type;

    std::expected<Result, std::error_code> try_invoke(Params... args) {
      return base_type::try_invoke(std::forward<Params>(args)...);
    }

    Result invoke(Params... args) {
      return base_type::invoke(std::forward<Params>(args)...);
    }

    Result operator()(Params... args) {
      return base_type::invoke(std::forward<Params>(args)...);
    }
  };

#ifdef OMNI_HAS_INLINE_SYSCALL
  template <typename T = omni::status>
    requires(omni::detail::is_x64)
  using inline_syscaller = syscaller<T, inline_syscaller_options>;
#endif

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

#ifdef OMNI_HAS_INLINE_SYSCALL
  template <typename T = omni::status, concepts::hash Hasher, typename... Args>
    requires(!concepts::function_pointer<T>)
  inline T inline_syscall(Hasher export_name, Args&&... args) {
    return inline_syscaller<T>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <typename T = omni::status, typename... Args>
    requires(!concepts::function_pointer<T>)
  inline T inline_syscall(default_hash export_name, Args&&... args) {
    return inline_syscall<T, default_hash>(export_name, std::forward<Args>(args)...);
  }

  template <auto Func, concepts::hash Hasher, class... Args>
  inline auto inline_syscall(Args&&... args) {
    constexpr Hasher func_name{detail::extract_function_name<Func>()};
    return inline_syscaller<decltype(Func)>{func_name}.invoke(std::forward<Args>(args)...);
  }

  template <auto Func, class... Args>
  inline auto inline_syscall(Args&&... args) {
    return inline_syscall<Func, default_hash>(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, concepts::hash Hasher, class... Args>
  inline auto inline_syscall(Hasher export_name, Args&&... args) {
    return inline_syscaller<F>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, class... Args>
  inline auto inline_syscall(default_hash export_name, Args&&... args) {
    return inline_syscall<F, default_hash>(export_name, std::forward<Args>(args)...);
  }
#endif

} // namespace omni
