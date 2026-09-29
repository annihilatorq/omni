#include "omni/kernel_modules.hpp"

#include <algorithm>
#include <print>
#include <ranges>

int main() {
  auto snapshot = omni::kernel_modules::snapshot();
  if (!snapshot) {
    std::println("Failed to enumerate kernel modules: {}", snapshot.error().message());
    return 1;
  }

  std::println("A kernel module snapshot becomes a normal forward range ({} modules):", snapshot->size());
  for (const auto& module : *snapshot | std::views::take(10)) {
    std::println("  base={:p} size={:#010x} name={}", module.base_address().ptr(), module.size(), module.name());
  }

  auto kernel =
    std::ranges::find_if(*snapshot, [](const omni::kernel_module& module) { return module.load_order_index() == 0; });
  if (kernel == snapshot->end()) {
    std::println("Kernel image was not found.");
    return 1;
  }

  std::println();
  std::println("The first module in load order is the kernel image:");
  std::println("  path : {}", (*kernel).path());
  std::println("  name : {}", (*kernel).name());
  std::println("  base : {:p}", (*kernel).base_address().ptr());
}
