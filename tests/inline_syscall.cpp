#include <Windows.h>

#include <cstdint>

#include "omni/error.hpp"
#include "omni/syscall.hpp"
#include "test_utils.hpp"

#ifdef OMNI_HAS_INLINE_SYSCALL

namespace {

  struct process_basic_information {
    void* reserved1{};
    void* peb_base_address{};
    void* reserved2[2]{};
    std::uintptr_t unique_process_id{};
    void* reserved3{};
  };

  using nt_query_information_process_fn = omni::status (*)(HANDLE, ULONG, void*, ULONG, ULONG*);

} // namespace

int main() {
  ut::suite inline_syscall_suite = [] {
    "inline syscaller reports syscall_id_not_found for non-syscall exports"_test = [] {
      HMODULE ntdll_handle = ::GetModuleHandleW(L"ntdll.dll");
      FARPROC rtl_get_version = ::GetProcAddress(ntdll_handle, "RtlGetVersion");
      omni::inline_syscaller<omni::status> caller{"RtlGetVersion"};
      auto result = caller.try_invoke();

      require(ntdll_handle != nullptr);
      require(rtl_get_version != nullptr);
      expect(not result.has_value());
      expect(result.error() == make_error_code(omni::error::syscall_id_not_found));
    };

    "inline syscaller matches NtQueryInformationProcess from ntdll"_test = [] {
      HMODULE ntdll_handle = ::GetModuleHandleW(L"ntdll.dll");
      auto direct_function =
        reinterpret_cast<nt_query_information_process_fn>(::GetProcAddress(ntdll_handle, "NtQueryInformationProcess"));
      omni::inline_syscaller<omni::status> caller{"NtQueryInformationProcess"};

      process_basic_information direct_info{};
      process_basic_information inline_info{};
      ULONG direct_return_length{};
      ULONG inline_return_length{};

      omni::status direct_status =
        direct_function(::GetCurrentProcess(), 0U, &direct_info, sizeof(direct_info), &direct_return_length);
      auto inline_status =
        caller.try_invoke(::GetCurrentProcess(), 0U, &inline_info, sizeof(inline_info), &inline_return_length);

      require(ntdll_handle != nullptr);
      require(direct_function != nullptr);
      expect(inline_status.has_value());

      expect(inline_status->value == direct_status.value);
      expect(inline_status->is_success());
      expect(direct_return_length == sizeof(direct_info));
      expect(inline_return_length == sizeof(inline_info));
      expect(inline_return_length == direct_return_length);
      expect(direct_info.peb_base_address != nullptr);
      expect(inline_info.peb_base_address == direct_info.peb_base_address);
      expect(inline_info.unique_process_id == direct_info.unique_process_id);
      expect(static_cast<DWORD>(inline_info.unique_process_id) == ::GetCurrentProcessId());
    };

    "typed inline syscaller and free inline syscall overloads match NtQueryInformationProcess"_test = [] {
      omni::fnv1a32 syscall_name{"NtQueryInformationProcess"};
      omni::inline_syscaller<nt_query_information_process_fn> typed_caller{syscall_name};

      process_basic_information typed_info{};
      process_basic_information free_typed_info{};
      process_basic_information free_generic_info{};
      ULONG typed_return_length{};
      ULONG free_typed_return_length{};
      ULONG free_generic_return_length{};

      auto typed_status =
        typed_caller.try_invoke(::GetCurrentProcess(), 0U, &typed_info, sizeof(typed_info), &typed_return_length);

      omni::status free_typed_status = omni::inline_syscall<nt_query_information_process_fn>(syscall_name,
        ::GetCurrentProcess(),
        0U,
        &free_typed_info,
        sizeof(free_typed_info),
        &free_typed_return_length);

      auto free_generic_status = omni::inline_syscall<omni::status>("NtQueryInformationProcess",
        ::GetCurrentProcess(),
        0U,
        &free_generic_info,
        sizeof(free_generic_info),
        &free_generic_return_length);

      require(typed_status.has_value());

      expect(typed_status->is_success());
      expect(free_typed_status.is_success());
      expect(free_generic_status.is_success());
      expect(typed_return_length == sizeof(typed_info));
      expect(free_typed_return_length == sizeof(free_typed_info));
      expect(free_generic_return_length == sizeof(free_generic_info));
      expect(typed_info.peb_base_address == free_typed_info.peb_base_address);
      expect(typed_info.peb_base_address == free_generic_info.peb_base_address);
      expect(typed_info.unique_process_id == free_typed_info.unique_process_id);
      expect(typed_info.unique_process_id == free_generic_info.unique_process_id);
    };
  };
}

#endif
