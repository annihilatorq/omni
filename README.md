# shadow syscalls

Easy to use syscall/import executor wrapper. Syscall is based on shellcode. Function names passed in arguments are hashed at compile-time.
Supports x86 architecture, but on x86 `.shadowsyscall()` is inaccessible.

The repository provides a convenient high-level wrapper over low-level operations, range-based enumerators for modules and their exports. Includes all `GetModuleHandle`, `GetProcAddress` implementations in a much nicer wrapper without leaving any strings in binary.
Allows calling undocumented DLL functions. Has a built-in forwarded import resolver (HeapAlloc, etc.)

### Supported platforms
CLANG, GCC, MSVC. Library requires cpp20.

### Quick example
```cpp
// Execute "NtTerminateProcess" syscall
shadowsyscall<NTSTATUS>( "NtTerminateProcess", reinterpret_cast< HANDLE >( -1 ), -6932 );

// Since version 1.2, the return type may not be specified
shadowsyscall( "NtTerminateProcess", reinterpret_cast< HANDLE >( -1 ), -6932 );

// Execute any export at runtime
// Since version 1.2, the return type may not be specified
shadowcall<int>( "MessageBoxA", nullptr, "string 1", "string 2", MB_OK );
```

> [!IMPORTANT]\
> Make sure you load the dll module that contains the export you want to call. For example - to call MessageBoxA, you need to load "user32.dll" into the current process.

Shellcode uses allocator based on `NtAllocateVirtualMemory` & `NtFreeVirtualMemory`

## Detailed executors example (x64)
```cpp
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
```

## Detailed module & shared-data parser example
```cpp
#include <string>
#include "shadowsyscall.hpp"

#define obfuscate_string(str)    \
  []() {                         \
    /* Any string obfuscation */ \
    return str;                  \
  }

int main() {
  // Enumerate every dll loaded into current process
  for (const auto& dll : shadow::dlls())
    std::cout << dll.filepath().string() << " : " << dll.native_handle() << "\n";

  std::cout.put('\n');

  // Find a specific DLL loaded into the current process.
  // "ntdll.dll" doesn't leave the string in the executable -
  // it’s hashed at compile time (consteval guarantee)
  // The implementation doesn't care about the ".dll" suffix

  auto ntdll = shadow::dll("ntdll" /* after compilation it will become 384989384324938 */);

  auto current_module = shadow::current_module();
  std::cout << "Current .exe filepath: " << current_module.filepath().string() << "\n";
  std::cout << "Current .text section checksum: "
            << current_module.section_checksum<std::size_t>(".text") << "\n\n";

  auto image = ntdll.image();
  auto nt_headers = image->get_nt_headers();
  auto optional_header = image->get_optional_header();

  std::cout << ntdll.base_address().ptr() << '\n';  // .base_address() returns address_t
  std::cout << ntdll.native_handle() << '\n';       // .native_handle() returns void*
  std::cout << ntdll.entry_point() << '\n';         // .entry_point() returns address_t, if present
  std::cout << ntdll.name().string() << '\n';       // .name() returns win::unicode_string
  std::cout << ntdll.filepath().to_path() << '\n';  // .filepath() returns win::unicode_string
  std::cout << nt_headers->signature << '\n';       // returns uint32_t, NT magic value
  std::cout << optional_header->size_image << "\n\n";  // returns uint32_t, loaded NTDLL image size

  constexpr int export_entries_count = 5;
  const auto exports = ntdll.exports() | std::views::take(export_entries_count);

  std::cout << export_entries_count << " exports of ntdll.dll:\n";
  for (const shadow::win::export_t& exp : exports) {
    const auto& [name, address, ordinal] = std::make_tuple(exp.name, exp.address, exp.ordinal);
    std::cout << name << " : " << address << " : " << ordinal << '\n';
  }

  std::cout.put('\n');

  auto it = ntdll.exports().find_if([](const shadow::win::export_t& export_data) -> bool {
    // after compilation becomes 384989384324938
    constexpr auto compiletime_hash = shadow::hash64_t{"NtQuerySystemInformation"};
    // operator() (called in runtime) accepts any range that have access by index
    const auto runtime_hash = shadow::hash64_t{}(export_data.name);
    return compiletime_hash == runtime_hash;
  });

  const shadow::win::export_t& export_data = *it;
  std::cout << "Found target export:\n"
            << export_data.name << " : " << export_data.address << "\n\n";

  // "location" returns a DLL struct that contains this export
  std::cout << "The DLL that contains the Sleep export is: "
            << shadow::exported_symbol("Sleep").location().name().to_path() << "\n\n";

  // shared_data parses KUSER_SHARED_DATA
  // The class is a high-level wrapper for parsing,
  // which saves you from pointer arithmetic

  auto shared = shadow::shared_data();

  std::cout << shared.safe_boot_enabled() << '\n';
  std::cout << shared.boot_id() << '\n';
  std::cout << shared.physical_pages_num() << '\n';
  std::cout << shared.kernel_debugger_present() << '\n';
  std::wcout << shared.system_root().to_path() << '\n';

  std::cout << shared.system().is_windows_11() << '\n';
  std::cout << shared.system().is_windows_10() << '\n';
  std::cout << shared.system().is_windows_7() << '\n';
  std::cout << shared.system().build_number() << '\n';
  std::cout << shared.system().formatted() << '\n';

  std::cout << shared.unix_epoch_timestamp().utc().time_since_epoch() << '\n';
  std::cout << shared.unix_epoch_timestamp().utc().format_iso8601() << '\n';
  std::cout << shared.unix_epoch_timestamp().local().time_since_epoch() << '\n';
  std::cout << shared.unix_epoch_timestamp().local().format_iso8601() << '\n';
  std::cout << shared.timezone_offset<std::chrono::seconds>() << "\n\n";

  // Iterators are compatible with the ranges library
  static_assert(std::bidirectional_iterator<shadow::detail::export_view::iterator>);
  static_assert(std::bidirectional_iterator<shadow::detail::module_view::iterator>);

  // Code below DOES NOT COMPILE. hash*_t requires a string literal
  // because the hashing of the string happens at compile time, so there
  // is no point in you obfuscating the string in any way, because it
  // will turn into a number and disappear from the binary after compilation.
  //
  // constexpr auto hash_that_causes_ct_error = shadow::hash64_t{string_obfuscator("string")};
  //
  // The right way to do it is:
  // constexpr auto valid_hash = shadow::hash64_t{"string"};
}
```

## Hardware processor parser
```cpp
#include <iomanip>
#include <iostream>
#include "shadowsyscall.hpp"

int main() {
    auto support_message = []( std::string_view isa_feature, bool is_supported ) {
        constexpr int width = 12;
        std::cout << std::left << std::setw( width ) << isa_feature << ( is_supported ? "[+]" : "[-]" ) << std::endl;
    };

    std::cout << shadow::cpu().vendor() << std::endl;
    std::cout << shadow::cpu().brand() << std::endl;

    const auto& caches = shadow::cpu().caches();

    // CPU caches parsing is supported for current processor
    if ( caches ) {
        std::cout << "L1 cache size:     " << caches->l1_size() << "\n";
        std::cout << "L2 cache size:     " << caches->l2_size() << "\n";
        std::cout << "L3 cache size:     " << caches->l3_size() << "\n";
        std::cout << "Total caches size: " << caches->total_size().as_bytes() << "\n";
    } else {
        // Otherwise - the library does not yet support parsing for the existing processor
        std::cout << "Cache parsing is not supported by `shadow` on your processor architecture\n";
    }

    support_message( "IS_INTEL", shadow::cpu().is_intel() );
    support_message( "IS_AMD", shadow::cpu().is_amd() );
    support_message( "ABM", shadow::cpu().supports_abm() );
    support_message( "ADX", shadow::cpu().supports_adx() );
    support_message( "AES", shadow::cpu().supports_aes() );
    support_message( "AVX", shadow::cpu().supports_avx() );
    support_message( "AVX2", shadow::cpu().supports_avx2() );
    support_message( "AVX512CD", shadow::cpu().supports_avx512cd() );
    support_message( "AVX512ER", shadow::cpu().supports_avx512er() );
    support_message( "AVX512F", shadow::cpu().supports_avx512f() );
    support_message( "AVX512PF", shadow::cpu().supports_avx512pf() );
    support_message( "BMI1", shadow::cpu().supports_bmi1() );
    support_message( "BMI2", shadow::cpu().supports_bmi2() );
    support_message( "CLFLUSH", shadow::cpu().supports_clflush() );
    support_message( "CMPXCHG16B", shadow::cpu().supports_cmpxchg16b() );
    support_message( "CX8", shadow::cpu().supports_cx8() );
    support_message( "ERMS", shadow::cpu().supports_erms() );
    support_message( "F16C", shadow::cpu().supports_f16c() );
    support_message( "FMA", shadow::cpu().supports_fma() );
    support_message( "FSGSBASE", shadow::cpu().supports_fsgsbase() );
    support_message( "FXSR", shadow::cpu().supports_fxsr() );
    support_message( "HLE", shadow::cpu().supports_hle() );
    support_message( "INVPCID", shadow::cpu().supports_invpcid() );
    support_message( "LAHF", shadow::cpu().supports_lahf() );
    support_message( "LZCNT", shadow::cpu().supports_lzcnt() );
    support_message( "MMX", shadow::cpu().supports_mmx() );
    support_message( "MMXEXT", shadow::cpu().supports_mmxext() );
    support_message( "MONITOR", shadow::cpu().supports_monitor() );
    support_message( "MOVBE", shadow::cpu().supports_movbe() );
    support_message( "MSR", shadow::cpu().supports_msr() );
    support_message( "OSXSAVE", shadow::cpu().supports_osxsave() );
    support_message( "PCLMULQDQ", shadow::cpu().supports_pclmulqdq() );
    support_message( "POPCNT", shadow::cpu().supports_popcnt() );
    support_message( "PREFETCHWT1", shadow::cpu().supports_prefetchwt1() );
    support_message( "RDRAND", shadow::cpu().supports_rdrand() );
    support_message( "RDSEED", shadow::cpu().supports_rdseed() );
    support_message( "RDTSCP", shadow::cpu().supports_rdtscp() );
    support_message( "RTM", shadow::cpu().supports_rtm() );
    support_message( "SEP", shadow::cpu().supports_sep() );
    support_message( "SHA", shadow::cpu().supports_sha() );
    support_message( "SSE", shadow::cpu().supports_sse() );
    support_message( "SSE2", shadow::cpu().supports_sse2() );
    support_message( "SSE3", shadow::cpu().supports_sse3() );
    support_message( "SSE4.1", shadow::cpu().supports_sse4_1() );
    support_message( "SSE4.2", shadow::cpu().supports_sse4_2() );
    support_message( "SSE4a", shadow::cpu().supports_sse4a() );
    support_message( "SSSE3", shadow::cpu().supports_ssse3() );
    support_message( "SYSCALL", shadow::cpu().supports_syscall() );
    support_message( "TBM", shadow::cpu().supports_tbm() );
    support_message( "XOP", shadow::cpu().supports_xop() );
    support_message( "XSAVE", shadow::cpu().supports_xsave() );
}
```

## 🚀 Features

- Caching each call (it is possible to disable caching)
- Enumerate every DLL loaded to current process
- Compute checksum of the DLL section (any) in runtime
- Find exactly known DLL loaded to current process
- Enumerate EAT of module
- Resolve PE-headers and directories of module
- Compile-time string hasher
- Hash seed is pseudo-randomized, based on header file location
- Syscall executor
- Overriding syscall SSN parser
- Execute any export at runtime
- Doesn't leave any imports in the executable
- CPU instruction-set support checker & cache parser

## 📜 What is a syscall in Windows?
![syscalls](https://github.com/user-attachments/assets/1719c073-669b-4e6b-b2ec-23850ba91dbc)

## Thanks to
invers1on :heart:

https://github.com/can1357/linux-pe
