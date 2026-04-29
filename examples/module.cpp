#include <Windows.h>

#include "omni/module.hpp"
#include "omni/modules.hpp"

#include <print>
#include <ranges>
#include <string_view>

namespace {

  [[nodiscard]] std::string_view name_of_export(const omni::named_export& export_entry) {
    return export_entry.name;
  }

} // namespace

int main() {
  auto self = omni::base_module();
  auto kernel32 = omni::get_module(L"kernel32.dll");
  auto* optional_header = self.image()->get_optional_header();

  std::println("Current image:");
  std::println("  name                 : {}", self.name());
  std::println("  path                 : {}", self.system_path().string());
  std::println("  base                 : {:#x}", self.base_address().value());
  std::println("  entry point          : {:#x}", self.entry_point().value());
  std::println("  size of image        : {}", optional_header->size_image);
  std::println("  named exports        : {}", self.named_exports().size());
  std::println("  ordinal exports      : {}", self.ordinal_exports().size());

  std::println();
  std::println("Kernel32 convenience helpers:");
  std::println("  name                 : {}", kernel32.name());
  std::println("  path                 : {}", kernel32.system_path().string());
  std::println(R"(  matches "kernel32"   : {})", kernel32.matches_name_hash(L"kernel32"));
  std::println(R"(  matches "KERNEL32.DLL": {})", kernel32.matches_name_hash(L"KERNEL32.DLL"));

  std::println();
  std::println("First five named exports from kernel32:");

  auto kernel32_exports = kernel32.named_exports();
  auto first_named_exports = kernel32_exports | std::views::transform(name_of_export) | std::views::take(5);
  for (std::string_view export_name : first_named_exports) {
    std::println("  {}", export_name);
  }
}
