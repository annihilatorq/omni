#include <Windows.h>

#include <boost/ut.hpp>
#include <iterator>
#include <span>
#include <vector>

#include "omni/module.hpp"

#include "test_utils.hpp"

namespace tests = omni::tests;

ut::suite<"omni::module_exports"> module_exports_suite = [] {
  "default constructed exports are empty"_test = [] {
    omni::module_exports exports{};

    expect(exports.directory() == nullptr);
    expect(exports.size() == 0U);
    expect(exports.begin() == exports.end());
    expect(exports.find("GetProcAddress") == exports.end());
    expect(exports.find(1U, omni::use_ordinal) == exports.end());
    expect(exports.find_if([](const omni::module_export&) { return true; }) == exports.end());
    expect(exports.name(0).empty());
    expect(exports.ordinal(0) == 0U);
    expect(not exports.address(0));
    expect(not exports.is_export_forwarded(omni::address{}));
  };

  "directory size and indexed access match the PE export directory"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::module_exports exports = version_module.exports();

    const IMAGE_EXPORT_DIRECTORY* export_directory = tests::get_export_directory(version_dll.handle);
    auto export_entries = tests::get_export_table_entries(version_dll.handle);
    auto* export_entry = find_export_by_name(export_entries, "GetFileVersionInfoSizeW");

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(export_directory != nullptr));
    expect(fatal(export_entry != nullptr));

    expect(omni::address{exports.directory()} == export_directory);
    expect(exports.size() == export_entries.size());
    expect(exports.name(export_entry->function_index) == export_entry->name);
    expect(exports.ordinal(export_entry->function_index) == export_entry->ordinal);
    expect(exports.address(export_entry->function_index) == export_entry->address);
    expect(exports.is_export_forwarded(export_entry->address) == export_entry->is_forwarded);
    expect(exports.name(exports.size()).empty());
    expect(exports.ordinal(exports.size()) == 0U);
    expect(not exports.address(exports.size()));
  };

  "all export iteration matches the function table order"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::module_exports exports = version_module.exports();

    auto export_entries = tests::get_export_table_entries(version_dll.handle);
    std::size_t function_index{};

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(not export_entries.empty()));

    for (const omni::module_export& export_entry : exports) {
      expect(function_index < export_entries.size());

      auto manual_export = export_entries[function_index];
      expect(export_entry.name == manual_export.name);
      expect(export_entry.address == manual_export.address);
      expect(export_entry.ordinal == manual_export.ordinal);
      expect(export_entry.is_forwarded == manual_export.is_forwarded);
      expect(export_entry.module_base == version_module.base_address());

      ++function_index;
    }

    expect(function_index == export_entries.size());

    auto last_export = exports.end();
    std::advance(last_export, -1);

    expect(last_export->name == export_entries.back().name);
    expect(last_export->address == export_entries.back().address);
    expect(last_export->ordinal == export_entries.back().ordinal);
  };

  "named export iteration matches the name table order"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::module_exports exports = version_module.exports();

    auto named_export_entries = tests::get_named_export_table_entries(version_dll.handle);
    std::size_t name_index{};

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(not named_export_entries.empty()));

    for (const omni::module_export& export_entry : exports.named()) {
      expect(name_index < named_export_entries.size());

      auto manual_export = named_export_entries[name_index];
      expect(not export_entry.name.empty());
      expect(export_entry.name == manual_export.name);
      expect(export_entry.address == manual_export.address);
      expect(export_entry.ordinal == manual_export.ordinal);
      expect(export_entry.is_forwarded == manual_export.is_forwarded);
      expect(export_entry.module_base == version_module.base_address());

      ++name_index;
    }

    expect(name_index == named_export_entries.size());

    auto last_named_export = exports.named().end();
    std::advance(last_named_export, -1);

    expect(last_named_export->name == named_export_entries.back().name);
    expect(last_named_export->address == named_export_entries.back().address);
    expect(last_named_export->ordinal == named_export_entries.back().ordinal);
  };

  "ordinal export iteration matches the function table order"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    auto exports = version_module.exports();

    auto export_entries = tests::get_export_table_entries(version_dll.handle);
    std::size_t function_index{};

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(not export_entries.empty()));

    for (const omni::module_export& export_entry : exports.ordinal()) {
      expect(function_index < export_entries.size());

      auto manual_export = export_entries[function_index];
      expect(export_entry.name.empty());
      expect(export_entry.address == manual_export.address);
      expect(export_entry.ordinal == manual_export.ordinal);
      expect(export_entry.is_forwarded == manual_export.is_forwarded);
      expect(export_entry.module_base == version_module.base_address());

      ++function_index;
    }

    expect(function_index == export_entries.size());

    auto last_ordinal_export = exports.ordinal().end();
    std::advance(last_ordinal_export, -1);

    expect(last_ordinal_export->name.empty());
    expect(last_ordinal_export->address == export_entries.back().address);
    expect(last_ordinal_export->ordinal == export_entries.back().ordinal);
  };

  "find and find_if match GetProcAddress for name and ordinal lookup"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    omni::module_exports exports = version_module.exports();

    auto export_entries = tests::get_export_table_entries(version_dll.handle);
    auto* export_entry = find_export_by_name(export_entries, "GetFileVersionInfoSizeW");
    FARPROC export_address = ::GetProcAddress(version_dll.handle, "GetFileVersionInfoSizeW");

    auto by_name = exports.find("GetFileVersionInfoSizeW");
    auto by_ordinal = exports.find(export_entry == nullptr ? 0U : export_entry->ordinal, omni::use_ordinal);
    auto by_predicate = exports.find_if(
      [export_address](const omni::module_export& module_export) { return module_export.address == export_address; });

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(export_entry != nullptr));
    expect(fatal(export_address != nullptr));

    expect(by_name != exports.end());
    expect(by_ordinal != exports.end());
    expect(by_predicate != exports.end());

    expect(by_name->name == "GetFileVersionInfoSizeW");
    expect(by_name->address == export_address);
    expect(by_name->ordinal == export_entry->ordinal);
    expect(by_name->module_base == version_module.base_address());

    expect(by_ordinal->name == by_name->name);
    expect(by_ordinal->address == by_name->address);
    expect(by_ordinal->ordinal == by_name->ordinal);

    expect(by_predicate->name == by_name->name);
    expect(by_predicate->address == by_name->address);
    expect(by_predicate->ordinal == by_name->ordinal);
  };

  "forwarded exports are detected like the export table says"_test = [] {
    HMODULE kernel32_handle = ::GetModuleHandleW(L"kernel32.dll");
    omni::module kernel32_module = tests::get_loaded_module(kernel32_handle);
    omni::module_exports exports = kernel32_module.exports();
    auto export_entries = tests::get_export_table_entries(kernel32_handle);
    auto* forwarded_export = find_first_forwarded_export(export_entries);
    omni::default_hash forwarded_export_hash{
      forwarded_export == nullptr ? 0U : omni::hash<omni::default_hash>(forwarded_export->name)};
    auto by_name = exports.find(forwarded_export_hash);
    auto by_ordinal = exports.find(forwarded_export == nullptr ? 0U : forwarded_export->ordinal, omni::use_ordinal);
    FARPROC resolved_address =
      forwarded_export == nullptr ? nullptr : ::GetProcAddress(kernel32_handle, forwarded_export->name.data());

    expect(fatal(kernel32_handle != nullptr));
    expect(fatal(kernel32_module.present()));
    expect(fatal(forwarded_export != nullptr));
    expect(fatal(resolved_address != nullptr));
    expect(by_name != exports.end());
    expect(by_ordinal != exports.end());

    expect(by_name->is_forwarded);
    expect(by_ordinal->is_forwarded);
    expect(exports.is_export_forwarded(by_name->address));
    expect(by_name->name == forwarded_export->name);
    expect(by_name->address == forwarded_export->address);
    expect(by_name->ordinal == forwarded_export->ordinal);
    expect(by_name->module_base == kernel32_module.base_address());
    expect(by_ordinal->name == by_name->name);
    expect(by_ordinal->address == by_name->address);
    expect(by_ordinal->ordinal == by_name->ordinal);
    expect(by_name->address != resolved_address);
  };

  "forwarded exports expose the original forwarder string"_test = [] {
    HMODULE kernel32_handle = ::GetModuleHandleW(L"kernel32.dll");
    omni::module kernel32_module = tests::get_loaded_module(kernel32_handle);
    omni::module_exports exports = kernel32_module.exports();
    auto export_entries = tests::get_export_table_entries(kernel32_handle);

    auto* forwarded_export = find_first_forwarded_export(export_entries);
    auto by_ordinal = exports.find(forwarded_export == nullptr ? 0U : forwarded_export->ordinal, omni::use_ordinal);

    auto raw_forwarder_string = forwarded_export == nullptr ? std::string_view{} : forwarded_export->address.ptr<const char>();
    auto expected_forwarder = omni::forwarder_string::parse(raw_forwarder_string);

    expect(fatal(kernel32_handle != nullptr));
    expect(fatal(kernel32_module.present()));
    expect(fatal(forwarded_export != nullptr));
    expect(by_ordinal != exports.end());

    expect(by_ordinal->is_forwarded);
    expect(expected_forwarder.present());
    expect(by_ordinal->forwarder_string.present());
    expect(by_ordinal->forwarder_string.module == expected_forwarder.module);
    expect(by_ordinal->forwarder_string.function == expected_forwarder.function);
    expect(by_ordinal->forwarder_string.is_ordinal() == expected_forwarder.is_ordinal());
  };
};
