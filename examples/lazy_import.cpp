#include <Windows.h>

#include "omni/lazy_import.hpp"

#include <array>
#include <filesystem>
#include <print>

namespace {

  using get_module_handle_w_fn = HMODULE(WINAPI*)(LPCWSTR);
  using get_system_directory_w_fn = UINT(WINAPI*)(LPWSTR, UINT);

} // namespace

int main() {
  auto get_module_handle = omni::lazy_importer<get_module_handle_w_fn>{"GetModuleHandleW"};
  HMODULE kernel32_handle = get_module_handle(L"kernel32.dll");
  auto get_module_handle_export = get_module_handle.named_export();

  if (kernel32_handle == nullptr || !get_module_handle_export.present()) {
    std::println("Failed to lazy-import GetModuleHandleW");
    return 1;
  }

  std::println("Typed lazy importer:");
  std::println("  result               : {:#x}", reinterpret_cast<std::uintptr_t>(kernel32_handle));
  std::println("  export address       : {:#x}", get_module_handle_export.address.value());
  std::println("  owning module        : {}", omni::get_module(get_module_handle_export.module_base).name());

  auto system_directory_buffer = std::array<wchar_t, MAX_PATH>{};
  auto system_directory_length = omni::lazy_import<get_system_directory_w_fn>({"GetSystemDirectoryW", "kernel32.dll"},
    system_directory_buffer.data(),
    static_cast<UINT>(system_directory_buffer.size()));

  auto system_directory = std::filesystem::path{
    std::wstring_view{system_directory_buffer.data(), system_directory_length},
  };

  std::println();
  std::println("Hash-pair overload:");
  std::println("  system directory     : {}", system_directory.string());

  auto process_id = omni::lazy_import<::GetCurrentProcessId>();

  std::println();
  std::println("Auto-function overload:");
  std::println("  current process id   : {}", process_id);

  omni::lazy_import<void>("SetLastError", 0xCAFEU);
  auto last_error = ::GetLastError();

  std::println();
  std::println("Generic return-type overload:");
  std::println("  GetLastError()       : 0x{:X}", last_error);

  auto missing_export = omni::lazy_importer<DWORD>{"MissingExportForExamples", "kernel32.dll"}.try_invoke();
  if (!missing_export) {
    std::println();
    std::println("Failure diagnostics stay explicit:");
    std::println("  {}", missing_export.error().message());
  }
}
