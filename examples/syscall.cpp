#include <Windows.h>

#include "omni/syscall.hpp"

#include <cstdint>
#include <print>

struct process_basic_information {
  void* reserved1{};
  void* peb_base_address{};
  void* reserved2[2]{};
  std::uintptr_t unique_process_id{};
  void* reserved3{};
};

using nt_query_info_process_fn = omni::status (*)(HANDLE, ULONG, void*, ULONG, ULONG*);

int main() {
  omni::syscaller<nt_query_info_process_fn> query_process{"NtQueryInformationProcess"};

  process_basic_information process_info{};
  ULONG return_length{};

  auto query_status = query_process.try_invoke(::GetCurrentProcess(), 0U, &process_info, sizeof(process_info), &return_length);
  if (!query_status) {
    std::println("Failed to resolve NtQueryInformationProcess: {}", query_status.error().message());
    return 1;
  }

  process_basic_information shortcut_process_info{};
  ULONG shortcut_return_length{};

  auto shortcut_status = omni::syscall<nt_query_info_process_fn>("NtQueryInformationProcess",
    ::GetCurrentProcess(),
    0U,
    &shortcut_process_info,
    sizeof(shortcut_process_info),
    &shortcut_return_length);

  std::println("Typed syscall wrapper around NtQueryInformationProcess:");
  std::println("  status               : 0x{:08X}", static_cast<std::uint32_t>(query_status->value));
  std::println("  success              : {}", query_status->is_success());
  std::println("  PEB                  : {:#x}", reinterpret_cast<std::uintptr_t>(process_info.peb_base_address));
  std::println("  process id           : {}", process_info.unique_process_id);
  std::println("  return length        : {}", return_length);

  std::println();

  std::println("Free overload with a typed function signature:");
  std::println("  status               : 0x{:08X}", static_cast<std::uint32_t>(shortcut_status.value));
  std::println("  same PEB             : {}", shortcut_process_info.peb_base_address == process_info.peb_base_address);
  std::println("  same process id      : {}", shortcut_process_info.unique_process_id == process_info.unique_process_id);
  std::println("  return length        : {}", shortcut_return_length);

  auto yield_status = omni::syscall<omni::status>("NtYieldExecution");

  std::println();

  std::println("Generic syscall overload:");
  std::println("  NtYieldExecution     : 0x{:08X}", static_cast<std::uint32_t>(yield_status.value));
  std::println("  success              : {}", yield_status.is_success());

  auto not_a_syscall = omni::syscaller<omni::status>{"RtlGetVersion"}.try_invoke();
  if (!not_a_syscall) {
    std::println();
    std::println("Diagnostics stay explicit when an export is not a syscall stub:");
    std::println("  {}", not_a_syscall.error().message());
  }
}
