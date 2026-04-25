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
  std::println("yield   : 0x{:08X}", static_cast<std::uint32_t>(yield_status.value));
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

#include <print>
#include <ranges>

int main() {
  auto kernel32 = omni::get_module(L"kernel32.dll");

  for (const auto& export_entry : kernel32.exports().named() | std::views::take(5)) {
    std::println("{}", export_entry.name);
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

#include <print>

int main() {
  auto process_id = omni::lazy_import<DWORD>("GetCurrentProcessId");
  auto get_module_handle = omni::lazy_importer<HMODULE(WINAPI*)(LPCWSTR)>{"GetModuleHandleW"};
  auto kernel32 = get_module_handle(L"kernel32.dll");

  if (kernel32 == nullptr) {
    return 1;
  }

  auto export_entry = get_module_handle.module_export();

  std::println("process id     : {}", process_id);
  std::println("result         : {:#x}", reinterpret_cast<std::uintptr_t>(kernel32));
  std::println("export address : {:#x}", export_entry.address.value());
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
#include "omni/syscall.hpp"

#include <cstdint>
#include <print>

int main() {
  auto status = omni::syscall("NtYieldExecution");
  auto typed_status = omni::syscaller<omni::status (*)()>{"NtYieldExecution"}.invoke();

  std::println("status  : 0x{:08X}", static_cast<std::uint32_t>(status.value));
  std::println("success : {}", status.is_success());
  std::println("typed   : 0x{:08X}", static_cast<std::uint32_t>(typed_status.value));
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
- String literals passed to hash-aware APIs can be converted at compile time through implicit hash constructors.
- The project is still evolving, so API refinements are expected while the library surface settles.
- Third-party notices are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Thanks to `receiver1`, `po0p`, and invers1on for the contributions, ideas, and help around the project.
