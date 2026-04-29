#pragma once

#include <expected>
#include <utility>

#include "omni/concepts/concepts.hpp"
#include "omni/detail/config.hpp"
#include "omni/detail/extract_function_name.hpp"
#include "omni/detail/fixed_string.hpp"
#include "omni/detail/normalize_pointer_argument.hpp"
#include "omni/error.hpp"
#include "omni/hash.hpp"
#include "omni/module_export.hpp"
#include "omni/modules.hpp"

#ifdef OMNI_HAS_CACHING
#  include "omni/detail/memory_cache.hpp"
#endif

namespace omni {

#ifdef OMNI_HAS_CACHING
  namespace detail {
    struct export_cache_key {
      // Use std::uint64_t as a key to store underlying value type of any hash
      std::uint64_t export_name;
      std::uint64_t module_name;

      [[nodiscard]] auto operator<=>(const export_cache_key&) const noexcept = default;
    };

    struct export_cache_key_hasher {
      [[nodiscard]] std::size_t operator()(const export_cache_key& key) const noexcept {
        std::uint64_t value = key.export_name;
        value ^= std::rotl(key.module_name, 32);
        value ^= 0x9E3779B97F4A7C15ULL;
        value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
        value ^= value >> 31U;
        return static_cast<std::size_t>(value);
      }
    };

    inline detail::memory_cache<export_cache_key, omni::named_export, export_cache_key_hasher> exports_cache;
  } // namespace detail
#endif

  template <typename T>
  class lazy_importer {
   public:
    explicit lazy_importer(concepts::hash auto export_name): export_(resolve_module_export(export_name)) {}
    explicit lazy_importer(default_hash export_name): export_(resolve_module_export(export_name)) {}

    template <concepts::hash Hasher>
    explicit lazy_importer(Hasher export_name, Hasher module_name): export_(resolve_module_export(export_name, module_name)) {}
    explicit lazy_importer(default_hash export_name, default_hash module_name)
      : export_(resolve_module_export(export_name, module_name)) {}

    template <typename... Args>
    std::expected<T, std::error_code> try_invoke(Args&&... args) {
      if (!export_) {
        return std::unexpected(export_.error());
      }

      if constexpr (std::is_void_v<T>) {
        std::ignore = export_->address.template invoke<void>(detail::normalize_pointer_argument(std::forward<Args>(args))...);
        return {};
      } else {
        auto result = export_->address.template invoke<T>(detail::normalize_pointer_argument(std::forward<Args>(args))...);
        return result.value_or(T{});
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

    [[nodiscard]] omni::named_export named_export() const noexcept {
      return export_.value_or(omni::named_export{});
    }

   private:
    template <concepts::hash Hasher>
    static std::expected<omni::named_export, std::error_code> resolve_module_export(Hasher export_name, Hasher module_name) {
#ifdef OMNI_HAS_CACHING
      detail::export_cache_key export_cache_key{
        .export_name = export_name.value(),
        .module_name = module_name.value(),
      };

      auto module_export = detail::exports_cache.try_get(export_cache_key);
      if (!module_export or !module_export->present() or !omni::modules{}.contains(module_export->module_base)) {
        omni::module module = omni::get_module(module_name);
        if (!module.present()) {
          return std::unexpected(omni::error::module_not_loaded);
        }

        omni::named_export fresh_export = omni::get_export(export_name, module);
        if (!fresh_export.present()) {
          return std::unexpected(omni::error::export_not_found);
        }

        detail::exports_cache.set(export_cache_key, fresh_export);
        return fresh_export;
      }

      return *module_export;
#else
      omni::module module = omni::get_module(module_name);
      if (!module.present()) {
        return std::unexpected(omni::error::module_not_loaded);
      }

      omni::named_export fresh_export = omni::get_export(export_name, module);
      if (!fresh_export.present()) {
        return std::unexpected(omni::error::export_not_found);
      }

      return fresh_export;
#endif
    }

    static std::expected<omni::named_export, std::error_code> resolve_module_export(concepts::hash auto export_name) {
#ifdef OMNI_HAS_CACHING
      detail::export_cache_key export_cache_key{.export_name = export_name.value()};
      auto module_export = detail::exports_cache.try_get(export_cache_key);

      // The export is missing from the cache, or its owning module was
      // unloaded. If the module is loaded again at a different base, we
      // refresh the cached export, this adds a fast O(n) loaded-module
      // check to each export lookup to detect stale cache entries
      if (!module_export or !module_export->present() or !omni::modules{}.contains(module_export->module_base)) {
        omni::named_export fresh_export = omni::get_export(export_name);
        if (!fresh_export.present()) {
          return std::unexpected(omni::error::export_not_found);
        }

        detail::exports_cache.set(export_cache_key, fresh_export);
        return fresh_export;
      }

      return *module_export;
#else
      omni::named_export fresh_export = omni::get_export(export_name);
      if (!fresh_export.present()) {
        return std::unexpected(omni::error::export_not_found);
      }

      return fresh_export;
#endif
    }

    std::expected<omni::named_export, std::error_code> export_;
  };

  template <typename T, typename... Params>
  class lazy_importer<T (*)(Params...)> : private lazy_importer<T> {
   public:
    using lazy_importer<T>::lazy_importer;
    using lazy_importer<T>::named_export;

    std::expected<T, std::error_code> try_invoke(Params... args) {
      return lazy_importer<T>::try_invoke(args...);
    }

    T invoke(Params... args) {
      return lazy_importer<T>::invoke(args...);
    }

    T operator()(Params... args) {
      return lazy_importer<T>::invoke(args...);
    }
  };

#if defined(OMNI_ARCH_X86)
  template <typename T, typename... Params>
  class lazy_importer<T(__stdcall*)(Params...)> : private lazy_importer<T> {
   public:
    using lazy_importer<T>::lazy_importer;
    using lazy_importer<T>::named_export;

    std::expected<T, std::error_code> try_invoke(Params... args) {
      return lazy_importer<T>::try_invoke(args...);
    }

    T invoke(Params... args) {
      return lazy_importer<T>::invoke(args...);
    }

    T operator()(Params... args) {
      return lazy_importer<T>::invoke(args...);
    }
  };
#endif

  template <typename T = void, concepts::hash Hasher, typename... Args>
  inline T lazy_import(Hasher export_name, Args&&... args) {
    return lazy_importer<T>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <typename T = void, typename... Args>
  inline T lazy_import(default_hash export_name, Args&&... args) {
    return lazy_import<T, default_hash>(export_name, std::forward<Args>(args)...);
  }

  template <typename T = void, concepts::hash Hasher, typename... Args>
  inline T lazy_import(omni::hash_pair<Hasher> export_and_module_names, Args&&... args) {
    auto [export_name, module_name] = export_and_module_names;
    return lazy_importer<T>{export_name, module_name}.invoke(std::forward<Args>(args)...);
  }

  template <typename T = void, typename... Args>
  inline T lazy_import(omni::hash_pair<> export_and_module_names, Args&&... args) {
    return lazy_import<T, default_hash>(export_and_module_names, std::forward<Args>(args)...);
  }

  template <auto Func, concepts::hash Hasher, class... Args>
  inline auto lazy_import(Args&&... args) {
    constexpr Hasher func_name{detail::extract_function_name<Func>()};
    return lazy_importer<decltype(Func)>{func_name}.invoke(std::forward<Args>(args)...);
  }

  template <auto Func, class... Args>
  inline auto lazy_import(Args&&... args) {
    return lazy_import<Func, default_hash>(std::forward<Args>(args)...);
  }

  template <auto Func, detail::fixed_string ModuleName, concepts::hash Hasher, class... Args>
  inline auto lazy_import(Args&&... args) {
    constexpr Hasher func_name{detail::extract_function_name<Func>()};
    constexpr Hasher module_name{ModuleName.view()};
    return lazy_importer<decltype(Func)>{func_name, module_name}.invoke(std::forward<Args>(args)...);
  }

  template <auto Func, detail::fixed_string ModuleName, class... Args>
  inline auto lazy_import(Args&&... args) {
    return lazy_import<Func, ModuleName, default_hash>(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, concepts::hash Hasher, class... Args>
  inline auto lazy_import(Hasher export_name, Args&&... args) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    return lazy_importer<F>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, class... Args>
  inline auto lazy_import(default_hash export_name, Args&&... args) {
    return lazy_import<F, default_hash>(export_name, std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, concepts::hash Hasher, class... Args>
  inline auto lazy_import(omni::hash_pair<Hasher> export_and_module_names, Args&&... args) {
    auto [export_name, module_name] = export_and_module_names;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    return lazy_importer<F>{export_name, module_name}.invoke(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, class... Args>
  inline auto lazy_import(omni::hash_pair<> export_and_module_names, Args&&... args) {
    return lazy_import<F, default_hash>(export_and_module_names, std::forward<Args>(args)...);
  }

} // namespace omni
