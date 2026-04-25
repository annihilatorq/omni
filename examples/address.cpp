#include <Windows.h>

#include "omni/address.hpp"
#include "omni/module.hpp"
#include "omni/modules.hpp"

#include <array>
#include <cstdint>
#include <print>
#include <ranges>
#include <span>

namespace {

  using get_current_process_id_fn = DWORD(WINAPI*)();

  [[nodiscard]] std::uint32_t times_ten(std::uint32_t value) {
    return value * 10U;
  }

} // namespace

int main() {
  auto values = std::array{1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U};
  auto values_address = omni::address{values};
  std::span values_view = values_address.span<std::uint32_t>(values.size());

  std::println("Typed pointer arithmetic on top of raw addresses:");
  std::println("  values.data()        : {:#x}", values_address.value());
  std::println("  second element       : {}", *values_address.offset<const std::uint32_t*>(sizeof(std::uint32_t)));
  std::println("  fifth element        : {}", *values_address.ptr<const std::uint32_t>(sizeof(std::uint32_t) * 4));

  std::println();

  std::println("A raw memory block can immediately become a span/range:");
  auto scaled_values = values_view | std::views::transform(times_ten);
  for (std::uint32_t value : scaled_values) {
    std::println("  {}", value);
  }

  auto* kernel32_handle = ::GetModuleHandleW(L"kernel32.dll");
  auto kernel32 = omni::get_module(L"kernel32.dll");
  auto get_current_process_id = omni::address{::GetProcAddress(kernel32_handle, "GetCurrentProcessId")};

  if (kernel32_handle == nullptr || !kernel32.present() || !get_current_process_id) {
    std::println("Failed to resolve kernel32.dll or GetCurrentProcessId.");
    return 1;
  }

  auto image_size = kernel32.image()->get_optional_header()->size_image;
  auto inside_kernel32 =
    get_current_process_id.is_in_range(kernel32.base_address(), kernel32.base_address().offset(image_size));

  auto process_id = get_current_process_id.invoke<DWORD>();
  auto typed_get_current_process_id = get_current_process_id.as<get_current_process_id_fn>();

  std::println();

  std::println("The same address object can also represent and invoke code:");
  std::println("  GetCurrentProcessId  : {:#x}", get_current_process_id.value());
  std::println("  inside kernel32 image: {}", inside_kernel32);
  std::println("  invoke<DWORD>()      : {}", process_id.value_or(0U));
  std::println("  as<F*>()             : {}", typed_get_current_process_id());
}
