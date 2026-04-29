#include <Windows.h>

#include <cstdint>
#include <optional>

#include "omni/error.hpp"
#include "omni/syscall.hpp"
#include "test_utils.hpp"

namespace {

  struct process_basic_information {
    void* reserved1{};
    void* peb_base_address{};
    void* reserved2[2]{};
    std::uintptr_t unique_process_id{};
    void* reserved3{};
  };

  using nt_query_information_process_fn = omni::status (*)(HANDLE, ULONG, void*, ULONG, ULONG*);
  using syscall_id_parser_result = std::expected<std::uint32_t, std::error_code>;

} // namespace

ut::suite<"omni::syscall"> syscall_suite = [] {
  "missing export reports export_not_found"_test = [] {
    omni::syscaller<omni::status> caller{"MissingSyscallForOmniTests"};
    auto result = caller.try_invoke();

    expect(not result.has_value());
    expect(result.error() == make_error_code(omni::error::export_not_found));
  };

  "non syscall exports report syscall_id_not_found"_test = [] {
    HMODULE ntdll_handle = ::GetModuleHandleW(L"ntdll.dll");
    FARPROC rtl_get_version = ::GetProcAddress(ntdll_handle, "RtlGetVersion");
    omni::syscaller<omni::status> caller{"RtlGetVersion"};
    auto result = caller.try_invoke();

    expect(fatal(ntdll_handle != nullptr));
    expect(fatal(rtl_get_version != nullptr));
    expect(not result.has_value());
    expect(result.error() == make_error_code(omni::error::syscall_id_not_found));
  };

  "custom parsers receive the export address and can delegate to the default parser"_test = [] {
    HMODULE ntdll_handle = ::GetModuleHandleW(L"ntdll.dll");
    FARPROC direct_function = ::GetProcAddress(ntdll_handle, "NtQueryInformationProcess");
    omni::default_hash syscall_name{"NtQueryInformationProcess"};

#ifdef OMNI_HAS_CACHING
    omni::detail::syscall_id_cache.clear();
#endif

    int parser_calls{};
    omni::address parsed_address{};
    auto parser = [&parser_calls, &parsed_address](const omni::named_export& mod_export) -> syscall_id_parser_result {
      ++parser_calls;
      parsed_address = mod_export.address;
      return omni::default_syscall_id_parser(mod_export);
    };

    omni::syscaller<omni::status> caller{syscall_name, parser};

    process_basic_information syscall_info{};
    ULONG syscall_return_length{};
    auto syscall_status =
      caller.try_invoke(::GetCurrentProcess(), 0U, &syscall_info, sizeof(syscall_info), &syscall_return_length);

    expect(fatal(ntdll_handle != nullptr));
    expect(fatal(direct_function != nullptr));
    expect(parser_calls == 1);
    expect(parsed_address == direct_function);
    expect(syscall_status.has_value());
    expect(syscall_status->is_success());
    expect(syscall_return_length == sizeof(syscall_info));
    expect(syscall_info.peb_base_address != nullptr);
    expect(static_cast<DWORD>(syscall_info.unique_process_id) == ::GetCurrentProcessId());

#ifdef OMNI_HAS_CACHING
    expect(omni::detail::syscall_id_cache.contains(syscall_name.value()));

    omni::syscaller<omni::status> cached_caller{syscall_name, parser};
    auto cached_status =
      cached_caller.try_invoke(::GetCurrentProcess(), 0U, &syscall_info, sizeof(syscall_info), &syscall_return_length);

    expect(parser_calls == 1);
    expect(cached_status.has_value());
    expect(cached_status->is_success());

    omni::detail::syscall_id_cache.clear();
#endif
  };

  "custom parser errors are propagated by syscaller"_test = [] {
    omni::default_hash syscall_name{"NtQueryInformationProcess"};

#ifdef OMNI_HAS_CACHING
    omni::detail::syscall_id_cache.clear();
#endif

    int parser_calls{};
    omni::address parsed_address{};
    auto parser = [&parser_calls, &parsed_address](const omni::named_export& mod_export) -> syscall_id_parser_result {
      ++parser_calls;
      parsed_address = mod_export.address;
      return std::unexpected(make_error_code(omni::error::syscall_id_not_found));
    };

    omni::syscaller<omni::status> caller{syscall_name, parser};
    auto result = caller.try_invoke();

    expect(parser_calls == 1);
    expect(parsed_address != nullptr);
    expect(not result.has_value());
    expect(result.error() == make_error_code(omni::error::syscall_id_not_found));

#ifdef OMNI_HAS_CACHING
    expect(not omni::detail::syscall_id_cache.contains(syscall_name.value()));

    omni::syscaller<omni::status> second_caller{syscall_name, parser};
    auto second_result = second_caller.try_invoke();

    expect(parser_calls == 2);
    expect(not second_result.has_value());
    expect(second_result.error() == make_error_code(omni::error::syscall_id_not_found));
#endif
  };

  "generic syscaller matches NtQueryInformationProcess from ntdll"_test = [] {
    HMODULE ntdll_handle = ::GetModuleHandleW(L"ntdll.dll");
    auto direct_function =
      reinterpret_cast<nt_query_information_process_fn>(::GetProcAddress(ntdll_handle, "NtQueryInformationProcess"));
    omni::syscaller<omni::status> caller{"NtQueryInformationProcess"};

    process_basic_information direct_info{};
    process_basic_information syscall_info{};
    ULONG direct_return_length{};
    ULONG syscall_return_length{};

    omni::status direct_status =
      direct_function(::GetCurrentProcess(), 0U, &direct_info, sizeof(direct_info), &direct_return_length);
    auto syscall_status =
      caller.try_invoke(::GetCurrentProcess(), 0U, &syscall_info, sizeof(syscall_info), &syscall_return_length);

    expect(fatal(omni::detail::is_x64));
    expect(fatal(ntdll_handle != nullptr));
    expect(fatal(direct_function != nullptr));
    expect(syscall_status.has_value());

    expect(syscall_status->value == direct_status.value);
    expect(syscall_status->is_success());
    expect(direct_return_length == sizeof(direct_info));
    expect(syscall_return_length == sizeof(syscall_info));
    expect(syscall_return_length == direct_return_length);
    expect(direct_info.peb_base_address != nullptr);
    expect(syscall_info.peb_base_address == direct_info.peb_base_address);
    expect(syscall_info.unique_process_id == direct_info.unique_process_id);
    expect(static_cast<DWORD>(syscall_info.unique_process_id) == ::GetCurrentProcessId());
  };

  "typed syscaller and free syscall overloads match NtQueryInformationProcess"_test = [] {
    omni::fnv1a32 syscall_name{"NtQueryInformationProcess"};
    omni::syscaller<nt_query_information_process_fn> typed_caller{syscall_name};

    process_basic_information typed_info{};
    process_basic_information free_typed_info{};
    process_basic_information free_generic_info{};
    ULONG typed_return_length{};
    ULONG free_typed_return_length{};
    ULONG free_generic_return_length{};

    auto typed_status =
      typed_caller.try_invoke(::GetCurrentProcess(), 0U, &typed_info, sizeof(typed_info), &typed_return_length);

    omni::status free_typed_status = omni::syscall<nt_query_information_process_fn>(syscall_name,
      ::GetCurrentProcess(),
      0U,
      &free_typed_info,
      sizeof(free_typed_info),
      &free_typed_return_length);

    auto free_generic_status = omni::syscall<omni::status>("NtQueryInformationProcess",
      ::GetCurrentProcess(),
      0U,
      &free_generic_info,
      sizeof(free_generic_info),
      &free_generic_return_length);

    expect(fatal(typed_status.has_value()));

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

#ifdef OMNI_HAS_CACHING

  "successful resolution populates the cache and cached ids can be reused"_test = [] {
    omni::detail::syscall_id_cache.clear();

    HMODULE ntdll_handle = ::GetModuleHandleW(L"ntdll.dll");
    auto direct_function =
      reinterpret_cast<nt_query_information_process_fn>(::GetProcAddress(ntdll_handle, "NtQueryInformationProcess"));
    omni::default_hash syscall_name{"NtQueryInformationProcess"};
    omni::default_hash cached_alias{"DefinitelyMissingSyscallButCached"};

    omni::syscaller<omni::status> first{syscall_name};
    omni::syscaller<omni::status> second{syscall_name};
    auto cached_syscall_id = omni::detail::syscall_id_cache.try_get(syscall_name.value());

    process_basic_information direct_info{};
    process_basic_information cached_info{};
    ULONG direct_return_length{};
    ULONG cached_return_length{};

    expect(fatal(ntdll_handle != nullptr));
    expect(fatal(direct_function != nullptr));
    expect(cached_syscall_id.has_value());
    expect(omni::detail::syscall_id_cache.contains(syscall_name.value()));
    expect(omni::detail::syscall_id_cache.size() == 1U);

    std::ignore = first;
    std::ignore = second;

    omni::detail::syscall_id_cache.set(cached_alias.value(), *cached_syscall_id);
    omni::syscaller<omni::status> cached_caller{cached_alias};

    omni::status direct_status =
      direct_function(::GetCurrentProcess(), 0U, &direct_info, sizeof(direct_info), &direct_return_length);
    auto cached_status =
      cached_caller.try_invoke(::GetCurrentProcess(), 0U, &cached_info, sizeof(cached_info), &cached_return_length);

    expect(omni::detail::syscall_id_cache.contains(cached_alias.value()));
    expect(omni::detail::syscall_id_cache.size() == 2U);
    expect(cached_status.has_value());
    expect(cached_status->value == direct_status.value);
    expect(cached_return_length == direct_return_length);
    expect(cached_info.peb_base_address == direct_info.peb_base_address);
    expect(cached_info.unique_process_id == direct_info.unique_process_id);
  };

  "failed resolutions do not populate the syscall id cache"_test = [] {
    omni::detail::syscall_id_cache.clear();

    omni::default_hash missing_syscall{"DefinitelyMissingSyscallForOmniCacheTests"};
    omni::default_hash non_syscall_export{"RtlGetVersion"};
    omni::syscaller<omni::status> missing_caller{missing_syscall};
    omni::syscaller<omni::status> non_syscall_caller{non_syscall_export};
    auto missing_result = missing_caller.try_invoke();
    auto non_syscall_result = non_syscall_caller.try_invoke();

    expect(not missing_result.has_value());
    expect(not non_syscall_result.has_value());
    expect(missing_result.error() == make_error_code(omni::error::export_not_found));
    expect(non_syscall_result.error() == make_error_code(omni::error::syscall_id_not_found));
    expect(not omni::detail::syscall_id_cache.contains(missing_syscall.value()));
    expect(not omni::detail::syscall_id_cache.contains(non_syscall_export.value()));
    expect(omni::detail::syscall_id_cache.size() == 0U);
  };

#endif
};
