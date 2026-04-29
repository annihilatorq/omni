#include <Windows.h>

#include <boost/ut.hpp>
#include <iterator>

#include "omni/concepts/export_range.hpp"
#include "omni/module.hpp"

#include "test_utils.hpp"

namespace tests = omni::tests;

static_assert(omni::concepts::export_range<omni::ordinal_exports, std::uint32_t, omni::ordinal_export>);

ut::suite<"omni::ordinal_exports"> ordinal_exports_suite = [] {
  "default constructed exports are empty"_test = [] {
    omni::ordinal_exports exports{};

    expect(exports.directory() == nullptr);
    expect(exports.size() == 0U);
    expect(exports.begin() == exports.end());
    expect(exports.find(1U) == exports.end());
    expect(exports.find_if([](const omni::ordinal_export&) { return true; }) == exports.end());
  };

  "directory and size match the PE function table"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::ordinal_exports exports = version_module.ordinal_exports();

    const IMAGE_EXPORT_DIRECTORY* export_directory = tests::get_export_directory(version_dll.handle);
    auto export_entries = tests::get_export_table_entries(version_dll.handle);

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(export_directory != nullptr));

    expect(omni::address{exports.directory()} == export_directory);
    expect(exports.size() == export_entries.size());
  };

  "iteration matches the PE function table order"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::ordinal_exports exports = version_module.ordinal_exports();

    auto export_entries = tests::get_export_table_entries(version_dll.handle);
    std::size_t function_index{};

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(not export_entries.empty()));

    for (const omni::ordinal_export& export_entry : exports) {
      expect(function_index < export_entries.size());

      const auto& manual_export = export_entries[function_index];
      expect(export_entry.ordinal == manual_export.ordinal);
      expect(export_entry.address == manual_export.address);
      expect(export_entry.is_forwarded() == manual_export.is_forwarded);
      expect(export_entry.module_base == version_module.base_address());

      ++function_index;
    }

    expect(function_index == export_entries.size());

    auto last_export = exports.end();
    std::advance(last_export, -1);

    expect(last_export->ordinal == export_entries.back().ordinal);
    expect(last_export->address == export_entries.back().address);
  };

  "find and find_if match GetProcAddress for ordinal lookup"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::ordinal_exports exports = version_module.ordinal_exports();

    auto export_entries = tests::get_export_table_entries(version_dll.handle);
    auto* export_entry = find_export_by_name(export_entries, "GetFileVersionInfoSizeW");
    FARPROC export_address = ::GetProcAddress(version_dll.handle, "GetFileVersionInfoSizeW");

    auto by_ordinal = exports.find(export_entry == nullptr ? 0U : export_entry->ordinal);
    auto by_predicate = exports.find_if(
      [export_address](const omni::ordinal_export& ordinal_export) { return ordinal_export.address == export_address; });

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(export_entry != nullptr));
    expect(fatal(export_address != nullptr));

    expect(by_ordinal != exports.end());
    expect(by_predicate != exports.end());

    expect(by_ordinal->ordinal == export_entry->ordinal);
    expect(by_ordinal->address == export_address);
    expect(by_ordinal->module_base == version_module.base_address());

    expect(by_predicate->ordinal == by_ordinal->ordinal);
    expect(by_predicate->address == by_ordinal->address);
    expect(by_predicate->module_base == by_ordinal->module_base);
  };

  "forwarded ordinal exports are detected like the export table says"_test = [] {
    HMODULE kernel32_handle = ::GetModuleHandleW(L"kernel32.dll");
    omni::module kernel32_module = tests::get_loaded_module(kernel32_handle);
    omni::ordinal_exports exports = kernel32_module.ordinal_exports();
    auto named_export_entries = tests::get_named_export_table_entries(kernel32_handle);
    auto* forwarded_export = find_first_forwarded_export(named_export_entries);
    auto by_ordinal = exports.find(forwarded_export == nullptr ? 0U : forwarded_export->ordinal);
    FARPROC resolved_address =
      forwarded_export == nullptr ? nullptr : ::GetProcAddress(kernel32_handle, forwarded_export->name.data());

    expect(fatal(kernel32_handle != nullptr));
    expect(fatal(kernel32_module.present()));
    expect(fatal(forwarded_export != nullptr));
    expect(fatal(resolved_address != nullptr));
    expect(by_ordinal != exports.end());

    expect(by_ordinal->is_forwarded());
    expect(by_ordinal->ordinal == forwarded_export->ordinal);
    expect(by_ordinal->address == forwarded_export->address);
    expect(by_ordinal->module_base == kernel32_module.base_address());
    expect(by_ordinal->address != resolved_address);
  };

  "forwarded ordinal exports expose the original forwarder string"_test = [] {
    HMODULE kernel32_handle = ::GetModuleHandleW(L"kernel32.dll");
    omni::module kernel32_module = tests::get_loaded_module(kernel32_handle);
    omni::ordinal_exports exports = kernel32_module.ordinal_exports();
    auto named_export_entries = tests::get_named_export_table_entries(kernel32_handle);

    auto* forwarded_export = find_first_forwarded_export(named_export_entries);
    auto by_ordinal = exports.find(forwarded_export == nullptr ? 0U : forwarded_export->ordinal);

    auto raw_forwarder_string = forwarded_export == nullptr ? std::string_view{} : forwarded_export->address.ptr<const char>();
    auto expected_forwarder =
      forwarded_export == nullptr ? omni::forwarder_string{} : omni::forwarder_string::parse(raw_forwarder_string);

    expect(fatal(kernel32_handle != nullptr));
    expect(fatal(kernel32_module.present()));
    expect(fatal(forwarded_export != nullptr));
    expect(by_ordinal != exports.end());

    expect(by_ordinal->is_forwarded());
    expect(expected_forwarder.present());
    expect(by_ordinal->forwarder_string.present());
    expect(by_ordinal->forwarder_string.module == expected_forwarder.module);
    expect(by_ordinal->forwarder_string.function == expected_forwarder.function);
    expect(by_ordinal->forwarder_string.is_ordinal() == expected_forwarder.is_ordinal());
  };
};
