#include "omni/status.hpp"

#include <array>
#include <cstdint>
#include <print>
#include <ranges>
#include <string_view>
#include <utility>

namespace {

  using named_ntstatus = std::pair<std::string_view, omni::ntstatus>;
  using named_status = std::pair<std::string_view, omni::status>;

  [[nodiscard]] std::string_view to_string(omni::severity severity) {
    switch (severity) {
    case omni::severity::success:
      return "success";
    case omni::severity::information:
      return "information";
    case omni::severity::warning:
      return "warning";
    case omni::severity::error:
      return "error";
    }

    return "unknown";
  }

  void print_status(std::string_view label, omni::status status) {
    std::println("  {:<18} raw=0x{:08X}  success={}  severity={}  facility=0x{:03X}  code=0x{:04X}",
      label,
      static_cast<std::uint32_t>(status.value),
      status.is_success(),
      to_string(status.severity()),
      static_cast<std::uint16_t>(status.facility()),
      static_cast<std::uint16_t>(status.code()));
  }

  [[nodiscard]] named_status make_named_status(named_ntstatus item) {
    omni::status status{};
    status = item.second;
    return {item.first, status};
  }

  [[nodiscard]] bool is_failure_status(const named_status& item) {
    return !item.second.is_success();
  }

} // namespace

int main() {
  constexpr std::array cases{
    named_ntstatus{"success", omni::ntstatus::success},
    named_ntstatus{"timeout", omni::ntstatus::timeout},
    named_ntstatus{"no_more_entries", omni::ntstatus::no_more_entries},
    named_ntstatus{"access_denied", omni::ntstatus::access_denied},
  };

  std::println("NTSTATUS decoding helpers:");
  for (auto [label, code] : cases) {
    omni::status status{};
    status = code;
    print_status(label, status);
  }

  std::println();

  std::println("Ranges make it easy to focus on the failure cases:");
  auto failure_cases = cases | std::views::transform(make_named_status) | std::views::filter(is_failure_status);
  for (auto [label, status] : failure_cases) {
    print_status(label, status);
  }
}
