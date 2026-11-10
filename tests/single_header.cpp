#include <Windows.h>

#include "../single_header/omni.hpp"

#include <ut/ut.hpp>

using namespace ut;

using get_current_process_id_fn = DWORD(WINAPI*)();

int main() {
  "single header exposes compile-time utilities"_test = [] {
    using namespace omni::literals;

    expect("hello omni"_fnv1a32 == 0x8D8BF418U);
    expect("hello omni"_fnv1a64 == 0x3FCE4B355136BDD8ULL);
  };

  "single header resolves and invokes a WinAPI export"_test = [] {
    omni::lazy_importer<get_current_process_id_fn> get_current_process_id{"GetCurrentProcessId", "kernel32.dll"};

    require(get_current_process_id.named_export().present());
    expect(get_current_process_id() == ::GetCurrentProcessId());
  };

  "single header closes an owned kernel handle"_test = [] {
    omni::unique_handle handle{::CreateEventW(nullptr, TRUE, FALSE, nullptr)};

    require(handle.valid());
    expect(handle.close() == omni::ntstatus::success);
    expect(not handle.valid());
  };
}
