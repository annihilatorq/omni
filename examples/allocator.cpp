#include "omni/allocator.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <print>
#include <ranges>
#include <span>

namespace {

  [[nodiscard]] std::uint32_t square(std::uint32_t value) {
    return value * value;
  }

  [[nodiscard]] unsigned to_unsigned(std::byte value) {
    return std::to_integer<unsigned>(value);
  }

} // namespace

int main() {
  auto page_allocator = omni::rw_allocator<std::byte>{};
  auto* page = page_allocator.allocate(4096);
  auto page_view = std::span<std::byte>{page, 4096};

  std::ranges::fill(page_view, std::byte{0});

  auto* numbers = reinterpret_cast<std::uint32_t*>(page);
  auto values = std::span<std::uint32_t>{numbers, 16};
  std::ranges::iota(values, 1U);

  std::println("NtAllocateVirtualMemory-backed storage with a standard allocator interface:");
  std::println("  page address         : {:#x}", reinterpret_cast<std::uintptr_t>(page));
  std::println("  element count        : {}", values.size());

  std::println();

  std::println("The allocated block can immediately feed spans and ranges:");
  for (auto value : values | std::views::transform(square) | std::views::take(8)) {
    std::println("  {}", value);
  }

  std::println();

  std::println("First 16 bytes after initialization:");
  for (auto byte_value : page_view.first(16) | std::views::transform(to_unsigned)) {
    std::print("{:02X} ", byte_value);
  }

  page_allocator.deallocate(page, 4096);
}
