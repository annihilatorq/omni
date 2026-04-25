#pragma once

#include <cassert>
#include <filesystem>
#include <format>
#include <iosfwd>
#include <string>
#include <string_view>

#include "omni/address.hpp"
#include "omni/concepts.hpp"
#include "omni/hash.hpp"
#include "omni/module_exports.hpp"
#include "omni/win/peb.hpp"
#include "omni/win/unicode_string.hpp"

namespace omni {

  class module {
   public:
    module() = default;
    explicit module(win::loader_table_entry* module_data): entry_(module_data) {}

    [[nodiscard]] win::loader_table_entry* entry() noexcept {
      return entry_;
    }

    [[nodiscard]] const win::loader_table_entry* loader_entry() const noexcept {
      return entry_;
    }

    [[nodiscard]] win::image* image() noexcept {
      return assert_entry()->base_address.ptr<win::image>();
    }

    [[nodiscard]] const win::image* image() const noexcept {
      return assert_entry()->base_address.ptr<win::image>();
    }

    [[nodiscard]] omni::address base_address() const noexcept {
      return assert_entry()->base_address;
    }

    [[nodiscard]] void* native_handle() const noexcept {
      return assert_entry()->base_address.ptr();
    }

    [[nodiscard]] omni::address entry_point() const noexcept {
      return assert_entry()->entry_point;
    }

    [[nodiscard]] std::string name() const {
      return assert_entry()->name.string();
    }

    [[nodiscard]] std::wstring_view wname() const noexcept {
      return static_cast<std::wstring_view>(assert_entry()->name);
    }

    [[nodiscard]] std::filesystem::path system_path(
      std::filesystem::path::format fmt = std::filesystem::path::auto_format) const {
      return assert_entry()->path.to_path(fmt);
    }

    [[nodiscard]] module_exports exports() const noexcept {
      return module_exports{assert_entry()->base_address};
    }

    [[nodiscard]] bool present() const noexcept {
      return entry_ != nullptr;
    }

    [[nodiscard]] bool operator==(const module& other) const noexcept {
      return entry_ == other.entry_;
    }

    [[nodiscard]] bool matches_name_hash(concepts::hash auto module_name_hash) const noexcept {
      return compare_hashed_module_name(module_name_hash, wname());
    }

    [[nodiscard]] bool matches_name_hash(default_hash module_name_hash) const noexcept {
      return compare_hashed_module_name(module_name_hash, wname());
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return present();
    }

    friend std::ostream& operator<<(std::ostream& os, const module& module) {
      return os << module.name();
    }

   private:
    template <concepts::hash Hasher>
    static bool compare_hashed_module_name(Hasher module_name_hash, std::wstring_view module_name) {
      constexpr Hasher dll_suffix_hash{L".dll"};

      // Fast path most of the time, hash and compare full names
      if (module_name_hash == omni::hash<Hasher>(module_name)) {
        return true;
      }

      // When name is shorter than ".dll" means that suffix is already stripped
      auto name_length = module_name.length();
      if (name_length <= 4) {
        return false;
      }

      // Skip modules with suffixes other than ".dll" and pure names that didn't match
      auto module_suffix_hash = omni::hash<Hasher>(module_name.substr(name_length - 4));
      if (module_suffix_hash != dll_suffix_hash) {
        return false;
      }

      // Trim ".dll" suffix and compare pure module names
      auto trimmed_name_hash = omni::hash<Hasher>(module_name.substr(0, name_length - 4));
      return module_name_hash == trimmed_name_hash;
    }

    [[nodiscard]] win::loader_table_entry* assert_entry() noexcept {
      assert(entry_ != nullptr);
      return entry_;
    }

    [[nodiscard]] const win::loader_table_entry* assert_entry() const noexcept {
      assert(entry_ != nullptr);
      return entry_;
    }

    win::loader_table_entry* entry_{nullptr};
  };

} // namespace omni

template <>
struct std::formatter<omni::module> : std::formatter<std::string_view> {
  auto format(const omni::module& module, std::format_context& ctx) const {
    std::string module_name;
    if (module.present()) {
      module_name = module.name();
    }
    return std::formatter<std::string_view>{}.format(module_name, ctx);
  }
};
