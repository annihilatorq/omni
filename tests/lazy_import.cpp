#include <Windows.h>

#include <boost/ut.hpp>
#include <cstdint>

#include "omni/lazy_import.hpp"

#include "omni/module_export.hpp"
#include "test_utils.hpp"

namespace tests = omni::tests;

namespace {

  using get_module_handle_w_fn = HMODULE(WINAPI*)(LPCWSTR);
  using get_file_version_info_size_w_fn = DWORD(WINAPI*)(LPCWSTR, LPDWORD);

#ifdef OMNI_HAS_CACHING

  template <typename Hasher>
  [[nodiscard]] omni::detail::export_cache_key make_export_cache_key(Hasher export_name) {
    return {
      .export_name = static_cast<std::uint64_t>(export_name.value()),
      .module_name = 0U,
    };
  }

  template <typename Hasher>
  [[nodiscard]] omni::detail::export_cache_key make_export_cache_key(Hasher export_name, Hasher module_name) {
    return {
      .export_name = static_cast<std::uint64_t>(export_name.value()),
      .module_name = static_cast<std::uint64_t>(module_name.value()),
    };
  }

  void clear_exports_cache() {
    omni::detail::exports_cache.clear();
  }

#endif

} // namespace

ut::suite<"omni::lazy_import"> lazy_import_suite = [] {
  "missing export reports export_not_found and returns a default value"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::lazy_importer<DWORD> importer{"MissingExportForOmniTests", "version.dll"};
    auto result = importer.try_invoke();

    expect(fatal(static_cast<bool>(version_dll)));

    expect(not importer.named_export().present());
    expect(not result.has_value());
    expect(result.error() == make_error_code(omni::error::export_not_found));
    expect(importer.invoke() == 0U);
  };

  "missing module reports module_not_loaded and returns a default value"_test = [] {
    omni::lazy_importer<DWORD> importer{"GetCurrentProcessId", "omni_missing_module_for_tests.dll"};
    auto result = importer.try_invoke();

    expect(not importer.named_export().present());
    expect(not result.has_value());
    expect(result.error() == make_error_code(omni::error::module_not_loaded));
    expect(importer.invoke() == 0U);
  };

  "generic importer resolves exports with alternate hash types"_test = [] {
    omni::fnv1a32 export_name{"GetModuleHandleW"};
    omni::fnv1a32 module_name{"kernel32.dll"};
    omni::lazy_importer<HMODULE> importer{export_name, module_name};
    auto result = importer.try_invoke(L"kernel32.dll");
    omni::module kernel32 = omni::get_module(module_name);
    omni::named_export expected_export = omni::get_export(export_name, kernel32);
    HMODULE direct_result = ::GetModuleHandleW(L"kernel32.dll");

    expect(fatal(direct_result != nullptr));
    expect(fatal(expected_export.present()));
    expect(result.has_value());

    expect(*result == direct_result);
    expect(importer.invoke(L"kernel32.dll") == direct_result);
    expect(importer.named_export().present());
    expect(importer.named_export().address == expected_export.address);
    expect(importer.named_export().module_base == expected_export.module_base);
    expect(importer.named_export().name == expected_export.name);
  };

  "void importer invokes a real WinAPI function"_test = [] {
    omni::lazy_importer<void> importer{"SetLastError"};
    auto first_result = importer.try_invoke(0x1234U);

    expect(fatal(importer.named_export().present()));
    expect(first_result.has_value());
    expect(::GetLastError() == 0x1234U);

    importer.invoke(0x4321U);

    expect(::GetLastError() == 0x4321U);
  };

  "typed lazy_importer invokes the target function and preserves export metadata"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    auto version_path = version_module.system_path();
    auto direct_function =
      reinterpret_cast<get_file_version_info_size_w_fn>(::GetProcAddress(version_dll.handle, "GetFileVersionInfoSizeW"));
    omni::lazy_importer<get_file_version_info_size_w_fn> importer{"GetFileVersionInfoSizeW", "version.dll"};
    omni::named_export expected_export = omni::get_export("GetFileVersionInfoSizeW", version_module);

    DWORD direct_handle{};
    DWORD lazy_handle{};
    DWORD operator_handle{};

    DWORD direct_result = direct_function(version_path.c_str(), &direct_handle);
    auto try_result = importer.try_invoke(version_path.c_str(), &lazy_handle);
    DWORD operator_result = importer(version_path.c_str(), &operator_handle);

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(direct_function != nullptr));
    expect(fatal(expected_export.present()));
    expect(try_result.has_value());

    expect(*try_result == direct_result);
    expect(operator_result == direct_result);
    expect(lazy_handle == direct_handle);
    expect(operator_handle == direct_handle);
    expect(importer.named_export().address == expected_export.address);
    expect(importer.named_export().module_base == expected_export.module_base);
    expect(importer.named_export().name == expected_export.name);
  };

  "free lazy_import overloads resolve name pair and function template forms"_test = [] {
    omni::hash_pair<> process_id_in_kernel32{"GetCurrentProcessId", "kernel32.dll"};
    DWORD direct_process_id = ::GetCurrentProcessId();
    HMODULE direct_kernel32 = ::GetModuleHandleW(L"kernel32.dll");

    auto by_return_type = omni::lazy_import<DWORD>("GetCurrentProcessId");
    HMODULE by_typed_hash_pair =
      omni::lazy_import<get_module_handle_w_fn>({"GetModuleHandleW", "kernel32.dll"}, L"kernel32.dll");
    auto by_function_hash_pair = omni::lazy_import<DWORD>(process_id_in_kernel32);
    DWORD by_auto_function = omni::lazy_import<::GetCurrentProcessId>();
    DWORD by_auto_function_with_module = omni::lazy_import<::GetCurrentProcessId, "kernel32.dll">();

    expect(fatal(direct_kernel32 != nullptr));

    expect(by_return_type == direct_process_id);
    expect(by_typed_hash_pair == direct_kernel32);
    expect(by_function_hash_pair == direct_process_id);
    expect(by_auto_function == direct_process_id);
    expect(by_auto_function_with_module == direct_process_id);
  };

#ifdef OMNI_HAS_CACHING

  "successful lookups populate distinct global and module-specific cache keys"_test = [] {
    clear_exports_cache();

    omni::default_hash export_name{"GetCurrentProcessId"};
    omni::default_hash module_name{"kernel32.dll"};
    auto global_key = make_export_cache_key(export_name);
    auto module_key = make_export_cache_key(export_name, module_name);

    omni::lazy_importer<DWORD> global_importer{export_name};
    omni::lazy_importer<DWORD> module_importer{export_name, module_name};

    expect(fatal(global_importer.named_export().present()));
    expect(fatal(module_importer.named_export().present()));

    expect(omni::detail::exports_cache.contains(global_key));
    expect(omni::detail::exports_cache.contains(module_key));
    expect(omni::detail::exports_cache.size() == 2U);
  };

  "failed lookups do not populate the cache"_test = [] {
    clear_exports_cache();

    tests::loaded_library version_dll{L"version.dll"};
    omni::default_hash export_name{"DefinitelyMissingExportForOmniCacheTests"};
    omni::default_hash module_name{"version.dll"};
    auto cache_key = make_export_cache_key(export_name, module_name);
    omni::lazy_importer<DWORD> importer{export_name, module_name};
    auto result = importer.try_invoke();

    expect(fatal(static_cast<bool>(version_dll)));
    expect(not result.has_value());
    expect(result.error() == make_error_code(omni::error::export_not_found));
    expect(not omni::detail::exports_cache.contains(cache_key));
    expect(omni::detail::exports_cache.size() == 0U);
  };

  "stale cache entries are replaced with fresh export locations"_test = [] {
    clear_exports_cache();

    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::default_hash export_name{"GetFileVersionInfoSizeW"};
    omni::default_hash module_name{"version.dll"};

    auto cache_key = make_export_cache_key(export_name, module_name);
    omni::named_export expected_export = omni::get_export(export_name, version_module);

    omni::detail::exports_cache.set(cache_key,
      omni::named_export{
        .name = "stale",
        .address = omni::address{1U},
        .module_base = omni::address{1U},
      });

    omni::lazy_importer<DWORD> importer{export_name, module_name};
    auto cached_export = omni::detail::exports_cache.try_get(cache_key);

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(expected_export.present()));
    expect(fatal(cached_export.has_value()));

    expect(importer.named_export().address == expected_export.address);
    expect(importer.named_export().module_base == expected_export.module_base);
    expect(cached_export->address == expected_export.address);
    expect(cached_export->module_base == expected_export.module_base);
  };

#endif
};
