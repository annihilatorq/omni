#include <Windows.h>

#include "omni/hash.hpp"
#include "omni/module_exports.hpp"
#include "omni/modules.hpp"

#include <print>
#include <ranges>
#include <string_view>
#include <utility>

namespace {

  struct loaded_library {
    explicit loaded_library(const wchar_t* module_name) noexcept
      : handle{::LoadLibraryExW(module_name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)} {
      if (handle == nullptr) {
        handle = ::LoadLibraryW(module_name);
      }
    }

    loaded_library(const loaded_library&) = delete;
    loaded_library& operator=(const loaded_library&) = delete;

    loaded_library(loaded_library&& other) noexcept: handle{std::exchange(other.handle, nullptr)} {}
    loaded_library& operator=(loaded_library&& other) noexcept {
      if (this != &other) {
        if (handle != nullptr) {
          ::FreeLibrary(handle);
        }
        handle = std::exchange(other.handle, nullptr);
      }
      return *this;
    }

    ~loaded_library() {
      if (handle != nullptr) {
        ::FreeLibrary(handle);
      }
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return handle != nullptr;
    }

    HMODULE handle{};
  };

  [[nodiscard]] omni::default_hash runtime_hash(std::string_view value) {
    return omni::default_hash{omni::hash<omni::default_hash>(value)};
  }

  [[nodiscard]] bool has_sh_prefix(const omni::module_export& export_entry) {
    return export_entry.name.starts_with("SH");
  }

  [[nodiscard]] bool is_named_forwarded_export(const omni::module_export& export_entry) {
    return export_entry.is_forwarded && !export_entry.name.empty();
  }

} // namespace

int main() {
  loaded_library shlwapi{L"shlwapi.dll"};
  loaded_library shcore{L"shcore.dll"};
  auto shlwapi_module = omni::get_module(L"shlwapi.dll");
  auto exports = shlwapi_module.exports();

  if (!shlwapi || !shlwapi_module.present()) {
    std::println("Failed to load shlwapi.dll.");
    return 1;
  }

  std::println("shlwapi.dll export table:");
  std::println("  total exports        : {}", exports.size());
  std::println("  first 10 SH* exports :");

  for (const auto& export_entry : exports.named() | std::views::filter(has_sh_prefix) | std::views::take(10)) {
    std::println("    {:>4} {}", export_entry.ordinal, export_entry.name);
  }

  std::println();
  std::println("Ordinal iteration gives the whole table without doing name lookups:");
  for (const auto& export_entry : exports.ordinal() | std::views::take(5)) {
    std::println("  {:>4} -> {:#x}", export_entry.ordinal, export_entry.address.value());
  }

  auto forwarded_export = omni::module_export{};
  auto shunicode_to_ansi_cp = exports.find("SHUnicodeToAnsiCP");
  if (shunicode_to_ansi_cp != exports.end() && shunicode_to_ansi_cp->is_forwarded) {
    forwarded_export = *shunicode_to_ansi_cp;
  } else {
    auto named_exports = exports.named();
    auto first_named_forwarder = std::ranges::find_if(named_exports, is_named_forwarded_export);

    if (first_named_forwarder != named_exports.end()) {
      forwarded_export = *first_named_forwarder;
    }
  }

  if (!forwarded_export.present()) {
    std::println();
    std::println("No forwarded exports were found.");
    return 0;
  }

  auto target_module_hash = runtime_hash(forwarded_export.forwarder_string.module);
  auto resolved_export =
    forwarded_export.forwarder_string.is_ordinal() ?
      omni::get_export(forwarded_export.forwarder_string.to_ordinal(), target_module_hash, omni::use_ordinal) :
      omni::get_export(runtime_hash(forwarded_export.forwarder_string.function), target_module_hash);

  std::println();
  std::println("Forwarded exports stay readable as plain data:");
  std::println("  raw export           : {}", forwarded_export.name);
  std::println("  forwarder string     : {}.{}",
    forwarded_export.forwarder_string.module,
    forwarded_export.forwarder_string.function);

  if (!resolved_export) {
    std::println("  resolved target      : target module is not loaded");
    return 0;
  }

  std::println("  resolved module      : {}", omni::get_module(resolved_export.module_base).name());
  if (resolved_export.name.empty()) {
    std::println("  resolved target      : ordinal {}", resolved_export.ordinal);
  } else {
    std::println("  resolved target      : {}", resolved_export.name);
  }
  std::println("  resolved address     : {:#x}", resolved_export.address.value());
}
