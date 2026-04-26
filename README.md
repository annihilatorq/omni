[![Windows MSVC](https://github.com/annihilatorq/shadow_syscall/actions/workflows/windows-msvc.yml/badge.svg)](https://github.com/annihilatorq/shadow_syscall/actions/workflows/windows-msvc.yml)
[![Windows Clang-cl](https://github.com/annihilatorq/shadow_syscall/actions/workflows/windows-clang-cl.yml/badge.svg)](https://github.com/annihilatorq/shadow_syscall/actions/workflows/windows-clang-cl.yml)
[![Windows GCC](https://github.com/annihilatorq/shadow_syscall/actions/workflows/windows-gcc.yml/badge.svg)](https://github.com/annihilatorq/shadow_syscall/actions/workflows/windows-gcc.yml)
[![Windows MSVC Without Exceptions](https://github.com/annihilatorq/shadow_syscall/actions/workflows/windows-msvc-no-exceptions.yml/badge.svg)](https://github.com/annihilatorq/shadow_syscall/actions/workflows/windows-msvc-no-exceptions.yml)

# omni

> A header-only C++23 library for Windows-focused low-level work: module/export inspection, lazy imports, syscalls, shared user data, and compile-time hashing.

`omni` gives you modern C++ wrappers around the parts of WinAPI work that are usually noisy: walking the loader list, parsing exports, resolving imports by hash, calling syscalls, and reading shared user data. It stays range-friendly where iteration matters, keeps the common call sites short, and offers typed wrappers as optional extra validation when you want them.

## Highlights

- Header-only CMake target: `omni::omni`
- Loader/module/export inspection with `omni::modules`, `omni::module`, and `omni::module_exports`
- Lazy imports and syscall calls with terse free-function overloads
- Optional typed wrappers via `omni::lazy_importer` and `omni::syscaller`
- Compile-time string hashing with implicit literal-friendly constructors
- Ranges-friendly iterators for modules and exports
- Optional caching for syscall IDs and lazy imports
- Windows-specific tests for loader semantics, export parsing, caching, and syscall resolution

## Quick Start

```cmake
add_subdirectory(path/to/omni)
target_link_libraries(your_target PRIVATE omni::omni)
```

```cpp
#include <Windows.h>

#include "omni/modules.hpp"
#include "omni/syscall.hpp"

#include <cstdint>
#include <print>

int main() {
  auto kernel32 = omni::get_module(L"kernel32.dll");
  auto yield_status = omni::syscall("NtYieldExecution");

  std::println("module  : {}", kernel32.name());
  std::println("exports : {}", kernel32.exports().size());
  std::println("yield   : 0x{:08X}", yield_status);
}
```

## API Surface

| Area | Main types |
| --- | --- |
| Syscalls | `omni::syscall`, `omni::syscaller`, `omni::status` |
| Lazy imports | `omni::lazy_import`, `omni::lazy_importer` |
| Loader inspection | `omni::modules`, `omni::module`, `omni::module_export`, `omni::module_exports` |
| Shared data | `omni::shared_user_data` |
| Utilities | `omni::address`, `omni::allocator`, `omni::fnv1a32`, `omni::fnv1a64`, custom hash policies |

## Examples

Every file in [`examples/`](examples) builds as its own executable:

`address` · `allocator` · `hash` · `lazy_import` · `module` · `module_exports` · `modules` · `shared_user_data` · `status` · `syscall`

<details>
  <summary><code>examples/module.cpp</code> — iterate exports like a normal range</summary>

```cpp
#include <Windows.h>

#include "omni/module.hpp"
#include "omni/modules.hpp"

#include <print>
#include <ranges>
#include <string_view>

namespace {

  [[nodiscard]] std::string_view name_of_export(const omni::module_export& export_entry) {
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
  std::println("  export count         : {}", self.exports().size());

  std::println();
  std::println("Kernel32 convenience helpers:");
  std::println("  name                 : {}", kernel32.name());
  std::println("  path                 : {}", kernel32.system_path().string());
  std::println(R"(  matches "kernel32"   : {})", kernel32.matches_name_hash(L"kernel32"));
  std::println(R"(  matches "KERNEL32.DLL": {})", kernel32.matches_name_hash(L"KERNEL32.DLL"));

  std::println();
  std::println("First five named exports from kernel32:");

  auto kernel32_exports = kernel32.exports();
  auto first_named_exports = kernel32_exports.named() | std::views::transform(name_of_export) | std::views::take(5);
  for (std::string_view export_name : first_named_exports) {
    std::println("  {}", export_name);
  }
}
```

Example output:

```text
AcquireSRWLockExclusive
AcquireSRWLockShared
ActivateActCtx
AddAtomA
AddAtomW
```

</details>

<details>
  <summary><code>examples/lazy_import.cpp</code> — simple call site first, typed access when you want metadata</summary>

```cpp
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
  auto get_module_handle_export = get_module_handle.module_export();

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
```

Example output:

```text
process id     : 18432
result         : 0x7ff9d3d40000
export address : 0x7ff9d3d5b7f0
```

</details>

<details>
  <summary><code>examples/syscall.cpp</code> — plain syscall first, typed wrappers when you want extra checking</summary>

```cpp
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
```

Example output:

```text
status  : 0x00000000
success : true
typed   : 0x00000000
```

</details>

For fuller examples, see:

- [`examples/hash.cpp`](examples/hash.cpp)
- [`examples/module_exports.cpp`](examples/module_exports.cpp)
- [`examples/modules.cpp`](examples/modules.cpp)
- [`examples/shared_user_data.cpp`](examples/shared_user_data.cpp)

## Building

Requirements:

- Windows
- CMake 3.21+
- A C++23 compiler

Build the library, all examples, and the test suite:

```bash
cmake -S . -B build -DOMNI_BUILD_EXAMPLES=ON -DOMNI_BUILD_TESTS=ON
cmake --build build --config Release
```

Useful CMake options:

| Option | Default | Meaning |
| --- | --- | --- |
| `OMNI_BUILD_EXAMPLES` | `ON` for top-level builds | Build every file in `examples/` as a standalone executable |
| `OMNI_BUILD_TESTS` | `ON` for top-level builds | Build the Windows test suite |
| `OMNI_DISABLE_EXCEPTIONS` | `OFF` | Disable C++ exceptions |

## Testing

The test suite lives in [`tests/`](tests) and builds one `omni_test_<name>` executable per test source, each registered with `CTest`.

Current coverage includes:

- loader iteration and lookup
- `module` identity and WinAPI-facing properties
- export table parsing, named/ordinal iteration, and forwarders
- lazy import success paths, failure paths, typed overloads, and caching
- syscall resolution, typed/generic wrappers, and cache behavior

Run the suite with CTest:

```bash
ctest --test-dir build --output-on-failure
```

If you use a multi-config generator, add `-C Release` or your chosen configuration.

Tests use [`boost.ut`](https://github.com/boost-ext/ut). If it is not already available, CMake fetches it automatically.

## Configuration

`omni` is mostly zero-config, but a few compile-time switches are available:

| Macro | Effect |
| --- | --- |
| `OMNI_DISABLE_CACHING` | Disable internal caching for lazy imports and syscall IDs |
| `OMNI_ENABLE_ERROR_STRINGS` | Keep readable error strings in non-debug builds |
| `OMNI_DISABLE_ERROR_STRINGS` | Strip error strings even in debug builds |

## Notes

- `syscall` examples assume an x64 Windows process.
- The hash arguments of the function are guaranteed to convert the string literal you pass into a number at compile time
- The project is still evolving, so API refinements are expected while the library surface settles.
- Third-party notices are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Thanks to `receiver1`, `po0p`, and invers1on for the contributions, ideas, and help around the project.
