#include <Windows.h>
#include <thread>
#include "shadowsyscall.hpp"

// If "set_custom_ssn_parser" was called, the handling
// of the syscall index is entirely the user's responsibility
//
// This function is gonna be called once if caching is enabled.
// If not, the function will be called on every syscall
std::optional<uint32_t> custom_ssn_parser(shadow::syscaller<NTSTATUS>& instance,
                                          shadow::address_t export_address) {
  if (!export_address) {
    instance.set_last_error(shadow::error::ssn_not_found);
    return std::nullopt;
  }
  return *export_address.ptr<std::uint32_t>(4);
}

// Pass the function name as a string, and it will be converted
// into a number at compile-time by the hash64_t constructor
void execute_syscall_with_custom_ssn_parser(shadow::hash64_t function_name) {
  shadow::syscaller<NTSTATUS> sc{function_name};
  sc.set_ssn_parser(custom_ssn_parser);

  auto current_process = reinterpret_cast<void*>(-1);
  std::uintptr_t debug_port{0};
  auto status = sc(current_process, 7, &debug_port, sizeof(std::uintptr_t), nullptr);
  if (auto error = sc.last_error(); error)
    std::cerr << "Syscall error occurred: " << error.value() << '\n';

  std::cout << "NtQueryInformationProcess status: 0x" << std::hex << status
            << ", debug port is: " << debug_port << "\n";
}

int main() {
  execute_syscall_with_custom_ssn_parser("NtQueryInformationProcess");

  // This is a replacement for: LoadLibraryA("user32.dll");
  //
  // Since LoadLibraryA is a function implemented in kernelbase.dll,
  // and kernelbase.dll is a "pinned" DLL module, it is,
  // guaranteed to be loaded into the process.
  shadowcall("LoadLibraryA", "user32.dll");

  // When we know which DLL the export is in, we can specify it so
  // that we don't have to iterate through the exports of all DLLs
  shadowcall({"MessageBoxA", "user32.dll"}, nullptr, "string 1", "string 2", MB_OK);

  // Get a wrapper for lazy importing, but with the
  // ability to get details of a DLL export
  shadow::importer<int> message_box_import("MessageBoxA");
  int message_box_result = message_box_import(nullptr, "string 3", "string 4", MB_OK);

  std::cout << "MessageBoxA returned: " << message_box_result
            << "; import data is: " << message_box_import.exported_symbol() << '\n';

  HANDLE thread_handle = nullptr;
  const auto current_process = reinterpret_cast<HANDLE>(-1);
  auto start_routine = [](void*) -> DWORD {
    std::cout << "\nHello from thread " << std::this_thread::get_id() << "\n";
    return 0;
  };

  // 1. Handle syscall failure
  // Return type may not be specified since v1.2
  shadow::syscaller<NTSTATUS> create_thread_sc("NtCreateThreadEx");

  auto create_thread_status = create_thread_sc(
      &thread_handle, THREAD_ALL_ACCESS, NULL, current_process,
      static_cast<LPTHREAD_START_ROUTINE>(start_routine), 0, FALSE, NULL, NULL, NULL, 0);

  if (auto error = create_thread_sc.last_error(); error)
    std::cout << "NtCreateThreadEx error occurred: " << error.value() << "\n";
  else
    std::cout << "NtCreateThreadEx call status: 0x" << std::hex << create_thread_status << '\n';

  // 2. When error handling is not required, get a plain return value
  auto simple_status = shadowsyscall("NtTerminateProcess", reinterpret_cast<HANDLE>(-1), -6932);
}
