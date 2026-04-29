#include <Windows.h>

#include <boost/ut.hpp>
#include <iterator>

#include "omni/concepts/export_range.hpp"
#include "omni/module.hpp"

#include "test_utils.hpp"

namespace tests = omni::tests;

static_assert(omni::concepts::export_range<omni::named_exports, omni::default_hash, omni::named_export>);

ut::suite<"omni::named_exports"> named_exports_suite = [] {
  "default constructed exports are empty"_test = [] {
    omni::named_exports exports{};

    expect(exports.directory() == nullptr);
    expect(exports.size() == 0U);
    expect(exports.begin() == exports.end());
    expect(exports.find("GetProcAddress") == exports.end());
    expect(exports.find_if([](const omni::named_export&) { return true; }) == exports.end());
  };

  "directory and size match the PE name table"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::named_exports exports = version_module.named_exports();

    const IMAGE_EXPORT_DIRECTORY* export_directory = tests::get_export_directory(version_dll.handle);
    auto named_export_entries = tests::get_named_export_table_entries(version_dll.handle);

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(export_directory != nullptr));

    expect(omni::address{exports.directory()} == export_directory);
    expect(exports.size() == named_export_entries.size());
  };

  "iteration matches the PE name table order"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::named_exports exports = version_module.named_exports();

    auto named_export_entries = tests::get_named_export_table_entries(version_dll.handle);
    std::size_t name_index{};

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(not named_export_entries.empty()));

    for (const omni::named_export& export_entry : exports) {
      expect(name_index < named_export_entries.size());

      const auto& manual_export = named_export_entries[name_index];
      expect(export_entry.name == manual_export.name);
      expect(export_entry.address == manual_export.address);
      expect(export_entry.is_forwarded() == manual_export.is_forwarded);
      expect(export_entry.module_base == version_module.base_address());

      ++name_index;
    }

    expect(name_index == named_export_entries.size());

    auto last_export = exports.end();
    std::advance(last_export, -1);

    expect(last_export->name == named_export_entries.back().name);
    expect(last_export->address == named_export_entries.back().address);
  };

  "find and find_if match GetProcAddress for name lookup"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::named_exports exports = version_module.named_exports();

    auto named_export_entries = tests::get_named_export_table_entries(version_dll.handle);
    auto* export_entry = find_export_by_name(named_export_entries, "GetFileVersionInfoSizeW");
    FARPROC export_address = ::GetProcAddress(version_dll.handle, "GetFileVersionInfoSizeW");

    auto by_name = exports.find("GetFileVersionInfoSizeW");
    auto by_predicate = exports.find_if(
      [export_address](const omni::named_export& named_export) { return named_export.address == export_address; });

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(export_entry != nullptr));
    expect(fatal(export_address != nullptr));

    expect(by_name != exports.end());
    expect(by_predicate != exports.end());

    expect(by_name->name == export_entry->name);
    expect(by_name->address == export_address);
    expect(by_name->module_base == version_module.base_address());

    expect(by_predicate->name == by_name->name);
    expect(by_predicate->address == by_name->address);
    expect(by_predicate->module_base == by_name->module_base);
  };

  "forwarded named exports are detected like the export table says"_test = [] {
    HMODULE kernel32_handle = ::GetModuleHandleW(L"kernel32.dll");
    omni::module kernel32_module = tests::get_loaded_module(kernel32_handle);
    omni::named_exports exports = kernel32_module.named_exports();
    auto named_export_entries = tests::get_named_export_table_entries(kernel32_handle);
    auto* forwarded_export = find_first_forwarded_export(named_export_entries);
    omni::default_hash forwarded_export_hash{
      forwarded_export == nullptr ? 0U : omni::hash<omni::default_hash>(forwarded_export->name)};
    auto by_name = exports.find(forwarded_export_hash);
    FARPROC resolved_address =
      forwarded_export == nullptr ? nullptr : ::GetProcAddress(kernel32_handle, forwarded_export->name.data());

    expect(fatal(kernel32_handle != nullptr));
    expect(fatal(kernel32_module.present()));
    expect(fatal(forwarded_export != nullptr));
    expect(fatal(resolved_address != nullptr));
    expect(by_name != exports.end());

    expect(by_name->is_forwarded());
    expect(by_name->name == forwarded_export->name);
    expect(by_name->address == forwarded_export->address);
    expect(by_name->module_base == kernel32_module.base_address());
    expect(by_name->address != resolved_address);
  };

  "forwarded named exports expose the original forwarder string"_test = [] {
    HMODULE kernel32_handle = ::GetModuleHandleW(L"kernel32.dll");
    omni::module kernel32_module = tests::get_loaded_module(kernel32_handle);
    omni::named_exports exports = kernel32_module.named_exports();
    auto named_export_entries = tests::get_named_export_table_entries(kernel32_handle);

    auto* forwarded_export = find_first_forwarded_export(named_export_entries);
    auto by_name =
      exports.find(forwarded_export == nullptr ? omni::default_hash{} : omni::hash<omni::default_hash>(forwarded_export->name));

    auto raw_forwarder_string = forwarded_export == nullptr ? std::string_view{} : forwarded_export->address.ptr<const char>();
    auto expected_forwarder =
      forwarded_export == nullptr ? omni::forwarder_string{} : omni::forwarder_string::parse(raw_forwarder_string);

    expect(fatal(kernel32_handle != nullptr));
    expect(fatal(kernel32_module.present()));
    expect(fatal(forwarded_export != nullptr));
    expect(by_name != exports.end());

    expect(by_name->is_forwarded());
    expect(expected_forwarder.present());
    expect(by_name->forwarder_string.present());
    expect(by_name->forwarder_string.module == expected_forwarder.module);
    expect(by_name->forwarder_string.function == expected_forwarder.function);
    expect(by_name->forwarder_string.is_ordinal() == expected_forwarder.is_ordinal());
  };
};
