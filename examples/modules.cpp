#include <Windows.h>

#include "omni/modules.hpp"

#include <print>
#include <ranges>
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

    HMODULE handle{};
  };

  [[nodiscard]] bool is_interesting_module(const omni::module& module) {
    return module.matches_name_hash(L"ntdll") || module.matches_name_hash(L"kernel32") ||
           module.matches_name_hash(L"version") || module.matches_name_hash(L"shlwapi") || module.matches_name_hash(L"shcore");
  }

  [[nodiscard]] bool has_dll_extension(const omni::module& module) {
    return module.wname().ends_with(L".dll") || module.wname().ends_with(L".DLL");
  }

  [[nodiscard]] bool has_many_exports(const omni::module& module) {
    return module.exports().size() > 1500;
  }

} // namespace

int main() {
  loaded_library version{L"version.dll"};
  loaded_library shlwapi{L"shlwapi.dll"};
  loaded_library shcore{L"shcore.dll"};

  omni::modules loaded_modules{};

  std::println("A loader list becomes a normal C++ range:");
  auto interesting_modules = loaded_modules | std::views::filter(is_interesting_module);
  for (const auto& module : interesting_modules) {
    std::println("  {:<16} base={:#x} exports={}", module.name(), module.base_address().value(), module.exports().size());
  }

  omni::modules after_process_image{};
  after_process_image.skip();

  std::println();

  std::println("The iterators compose nicely with views:");
  auto first_dlls = after_process_image | std::views::filter(has_dll_extension) | std::views::take(5);
  for (const auto& module : first_dlls) {
    std::println("  {}", module.name());
  }

  auto first_large_export_module = loaded_modules.find_if(has_many_exports);

  std::println();
  if (first_large_export_module != loaded_modules.end()) {
    std::println("First module with more than 1500 exports: {}", first_large_export_module->name());
  }

  auto kernel32 = loaded_modules.find(L"kernel32");
  if (kernel32 != loaded_modules.end()) {
    std::println();
    std::println("Find/contains work with loader names and base addresses:");
    std::println(R"(  contains("kernel32")  : {})", loaded_modules.contains(L"kernel32"));
    std::println("  contains(base)        : {}", loaded_modules.contains(kernel32->base_address()));
    std::println("  kernel32 path         : {}", kernel32->system_path().string());
  }
}
