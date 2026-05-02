[![Windows MSVC](https://github.com/annihilatorq/omni/actions/workflows/windows-msvc.yml/badge.svg)](https://github.com/annihilatorq/omni/actions/workflows/windows-msvc.yml)
[![Windows Clang-cl](https://github.com/annihilatorq/omni/actions/workflows/windows-clang-cl.yml/badge.svg)](https://github.com/annihilatorq/omni/actions/workflows/windows-clang-cl.yml)
[![Windows GCC](https://github.com/annihilatorq/omni/actions/workflows/windows-gcc.yml/badge.svg)](https://github.com/annihilatorq/omni/actions/workflows/windows-gcc.yml)
[![Windows MSVC Without Exceptions](https://github.com/annihilatorq/omni/actions/workflows/windows-msvc-no-exceptions.yml/badge.svg)](https://github.com/annihilatorq/omni/actions/workflows/windows-msvc-no-exceptions.yml)

# omni
<img width="1920" height="302" alt="Omni" src="https://github.com/user-attachments/assets/646760a2-b0d6-4302-a9e4-56d3fe50ce3d" />

> A header-only C++23 library for Windows loader inspection, export parsing, lazy imports, API-set lookups, syscalls, shared user data, and low-level address utilities.

`omni` wraps the Windows-native pieces that are usually noisy to use directly: loader walks, PE export parsing, forwarded exports, API-set schema lookups, lazy imports, syscall stubs, `KUSER_SHARED_DATA`, and compile-time hashing. The API stays range-friendly where iteration matters and keeps the high-frequency call sites short.

## Highlights

- Header-only CMake target: `omni::omni`
- Umbrella include: `#include "omni/omni.hpp"`
- Loader inspection with `omni::modules`, `omni::module`, `omni::base_module`, and `omni::get_module`
- Split export views with `omni::named_exports` and `omni::ordinal_exports`
- Forwarder-aware lookup helpers via `omni::get_export(...)`
- Lazy imports via `omni::lazy_import` and `omni::lazy_importer`
- x64 syscall wrappers via `omni::syscall` and `omni::syscaller`, plus `omni::inline_syscall` and `omni::inline_syscaller` on supported toolchains
- API-set schema access via `omni::api_set`, `omni::api_sets`, and `omni::get_api_set`
- Utility types for raw addresses, NT allocation, hashing, NTSTATUS decoding, and shared user data
- Optional caching for lazy imports and syscall IDs

## Quick Start

```cmake
add_subdirectory(path/to/omni)
target_link_libraries(your_target PRIVATE omni::omni)
```

```cpp
#include <Windows.h>

#include "omni/omni.hpp"

#include <print>

int main() {
  auto kernel32 = omni::get_module(L"kernel32.dll");
  auto get_module_handle = omni::get_export("GetModuleHandleW", kernel32);
  auto process_id = omni::lazy_import<::GetCurrentProcessId>();

  if (!kernel32.present() || !get_module_handle.present()) {
    return 1;
  }

  std::println("module        : {}", kernel32.name());
  std::println("named exports : {}", kernel32.named_exports().size());
  std::println("ordinal count : {}", kernel32.ordinal_exports().size());
  std::println("process id    : {}", process_id);
  std::println("GetModuleHandleW @ {:#x}", get_module_handle.address.value());
}
```

If you prefer an amalgamated distribution, the generated single-header build lives at [`single_header/omni.hpp`](single_header/omni.hpp).

## API Overview

| Area | Main entry points |
| --- | --- |
| Loader/module inspection | `omni::modules`, `omni::module`, `omni::base_module`, `omni::get_module` |
| Export types | `omni::named_export`, `omni::ordinal_export`, `omni::forwarder_string`, `omni::use_ordinal` |
| Export lookup | `module.named_exports()`, `module.ordinal_exports()`, `omni::get_export(...)` |
| Lazy imports | `omni::lazy_import`, `omni::lazy_importer` |
| Syscalls | `omni::syscall`, `omni::syscaller`, `omni::inline_syscall`, `omni::inline_syscaller`, `omni::status`, `omni::ntstatus` |
| API sets | `omni::api_set`, `omni::api_sets`, `omni::get_api_set` |
| Utilities | `omni::address`, `omni::rw_allocator`, `omni::rx_allocator`, `omni::rwx_allocator`, `omni::fnv1a32`, `omni::fnv1a64`, `omni::hash_pair` |
| Shared data | `omni::shared_user_data` |

On supported x64 Clang/GCC-family toolchains, `omni::inline_syscall` and `omni::inline_syscaller` expose the same syscall surface while issuing the `syscall` instruction directly instead of going through a shellcode stub:

```cpp
int main() { 
  return static_cast<int>(omni::inline_syscall<omni::status>("NtYieldExecution"));
}
```

## Export Enumeration Contract

The export API is split intentionally:

| API | Iterates | `size()` means | `find(...)` key | Value type |
| --- | --- | --- | --- | --- |
| `module.named_exports()` | The PE name table | `IMAGE_EXPORT_DIRECTORY::NumberOfNames` | hashed export name | `omni::named_export` |
| `module.ordinal_exports()` | The PE function table | `IMAGE_EXPORT_DIRECTORY::NumberOfFunctions` | real export ordinal | `omni::ordinal_export` |

This means:

- `named_exports()` only sees exports that actually have names.
- `ordinal_exports()` sees the full export address table, including ordinal-only entries.
- `named_exports().size()` can be smaller than `ordinal_exports().size()`.
- Iterator order matches the underlying PE tables, not a re-sorted or normalized view.
- `ordinal_exports().find(ordinal)` expects the real ordinal value, not a zero-based index.

Both enumerators expose raw export-table data. Forwarded entries stay visible as forwarded entries:

- `named_export` carries `name`, `address`, `forwarder_string`, `forwarder_api_set`, and `module_base`
- `ordinal_export` carries `ordinal`, `address`, `forwarder_string`, `forwarder_api_set`, and `module_base`
- `is_forwarded()` tells you whether the entry points at a forwarder string instead of executable code

`omni::get_export(...)` sits one level higher than raw enumeration:

- Name-based overloads search `named_exports()`
- Ordinal overloads search `ordinal_exports()` and require the `omni::use_ordinal` tag
- Forwarded exports are resolved automatically when the target module or API-set host is already available
- If a forwarder cannot be fully resolved, you still get the original forwarded entry metadata instead of a silently rewritten result

```cpp
auto module = omni::get_module(L"kernel32.dll");

auto named = module.named_exports();
auto ordinal = module.ordinal_exports();

auto by_name = omni::get_export("GetCurrentProcessId", module);
auto first_ordinal = ordinal.begin()->ordinal;
auto by_ordinal = omni::get_export(first_ordinal, module, omni::use_ordinal);
```

## Examples

Every file in [`examples/`](examples) builds as a standalone executable:

| Example | What it demonstrates |
| --- | --- |
| [`examples/address.cpp`](examples/address.cpp) | Typed pointer arithmetic, range conversion, address-range checks, and invoking code through `omni::address` |
| [`examples/allocator.cpp`](examples/allocator.cpp) | `NtAllocateVirtualMemory`-backed standard allocator wrappers |
| [`examples/api_set.cpp`](examples/api_set.cpp) | Enumerating the live API-set schema and resolving default or alias-specific hosts |
| [`examples/hash.cpp`](examples/hash.cpp) | Built-in FNV-1a hashes, custom hashers, hash pairs, and range-based hashing pipelines |
| [`examples/lazy_import.cpp`](examples/lazy_import.cpp) | Typed and generic lazy imports, auto-function overloads, and explicit failure diagnostics |
| [`examples/module.cpp`](examples/module.cpp) | Inspecting the current image and basic module helpers |
| [`examples/module_exports.cpp`](examples/module_exports.cpp) | Raw named/ordinal export views, forwarded exports, and resolving forwarded targets |
| [`examples/modules.cpp`](examples/modules.cpp) | Walking the loader list as a normal C++ range |
| [`examples/shared_user_data.cpp`](examples/shared_user_data.cpp) | Reading `KUSER_SHARED_DATA` through a thin typed wrapper |
| [`examples/inline_syscall.cpp`](examples/inline_syscall.cpp) | Direct inline syscall wrappers on supported x64 Clang/GCC-style toolchains |
| [`examples/status.cpp`](examples/status.cpp) | Decoding `NTSTATUS` severity, facility, and code fields |
| [`examples/syscall.cpp`](examples/syscall.cpp) | Typed and generic syscall wrappers over live `ntdll` stubs |

## Building

Requirements:

- Windows
- CMake 3.21+
- A C++23 compiler

Top-level builds enable examples and tests by default. A minimal configure/build looks like this:

```bash
cmake -S . -B build
cmake --build build --config Release
```

Useful CMake options:

| Option | Default | Meaning |
| --- | --- | --- |
| `OMNI_BUILD_EXAMPLES` | `ON` for top-level builds | Build every file in `examples/` as a standalone executable |
| `OMNI_BUILD_TESTS` | `ON` for top-level builds | Build the unit-test executables and register them with `CTest` |
| `OMNI_DISABLE_EXCEPTIONS` | `OFF` | Build consumers with exceptions disabled through the interface target |

Notes:

- `examples/syscall.cpp` and `tests/syscall.cpp` are skipped automatically on x86 builds.
- `examples/inline_syscall.cpp` and `tests/inline_syscall.cpp` are skipped automatically when inline syscall support is unavailable.
- Under MinGW/libstdc++, the example targets link `stdc++exp` automatically for `std::print`.

## Testing

The test suite lives in [`tests/`](tests). Each `.cpp` test source becomes its own `omni_test_<name>` executable and is registered with `CTest`.

Current coverage includes:

- loader iteration, lookup, and base-module helpers
- `module` identity, paths, names, and image metadata
- raw named and ordinal export enumeration
- forwarded exports and `get_export(...)` resolution through normal modules and API sets
- lazy import success paths, failure paths, typed overloads, and cache behavior
- syscall resolution, custom parsers, typed/generic wrappers, inline syscall wrappers, and cache behavior
- API-set contract lookup and host resolution

Run the suite with:

```bash
ctest --test-dir build --output-on-failure
```

If you use a multi-config generator, add `-C Release` or your chosen configuration.

Tests use [`boost.ut`](https://github.com/boost-ext/ut). If it is not already available, CMake fetches it automatically.

## Configuration

`omni` is mostly zero-config, but a few compile-time switches matter:

| Macro | Effect |
| --- | --- |
| `OMNI_DISABLE_CACHING` | Disable internal caches for lazy imports and syscall IDs |
| `OMNI_ENABLE_ERROR_STRINGS` | Keep human-readable `std::error_code` messages in non-debug builds |
| `OMNI_DISABLE_ERROR_STRINGS` | Strip error strings even in debug builds |
| `OMNI_DISABLE_EXCEPTIONS` | Disable exception-based paths such as allocator `std::bad_alloc` throwing |

## Notes

- The library is Windows-specific.
- Syscall helpers are x64-only and rely on recognizable `ntdll` syscall stubs.
- On supported x64 Clang/GCC-style toolchains, `omni::inline_syscall` and `omni::inline_syscaller` issue the syscall instruction directly instead of going through a shellcode stub.
- Hashes are ASCII case-insensitive, which makes them convenient for module and export names.
- `api_sets::find(...)` accepts canonical contract names, versionless forms, and `.dll`-suffixed loader-style names.
- Third-party notices are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Thanks to `receiver1`, `po0p`, and `invers1on` for the ideas, contributions, and help around the project.
