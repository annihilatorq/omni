#include <Windows.h>

#include <filesystem>
#include <sstream>
#include <string>

#include "omni/module.hpp"
#include "test_utils.hpp"

namespace tests = omni::tests;

ut::suite<"omni::module"> module_suite = [] {
  "default constructed module is empty"_test = [] {
    omni::module module{};
    omni::module other{};

    expect(not module.present());
    expect(not static_cast<bool>(module));
    expect(module.entry() == nullptr);
    expect(module == other);
  };

  "copied module keeps the original loader entry identity"_test = [] {
    HMODULE executable_handle = ::GetModuleHandleW(nullptr);
    omni::module executable_module = tests::get_loaded_module(executable_handle);
    omni::module reconstructed{executable_module.entry()};

    expect(fatal(executable_handle != nullptr));
    expect(fatal(executable_module.present()));

    expect(reconstructed.present());
    expect(reconstructed.entry() == executable_module.entry());
    expect(reconstructed == executable_module);
  };

  "base address image and entry point match the PE image"_test = [] {
    HMODULE executable_handle = ::GetModuleHandleW(nullptr);
    omni::module executable_module = tests::get_loaded_module(executable_handle);
    omni::win::image* image = executable_module.image();
    omni::address expected_entry_point = executable_module.base_address().offset(image->get_optional_header()->entry_point);

    expect(fatal(executable_handle != nullptr));
    expect(fatal(executable_module.present()));

    expect(executable_module.base_address() == executable_handle);
    expect(executable_module.native_handle() == executable_handle);
    expect(image == executable_module.base_address().ptr<omni::win::image>());
    expect(image->get_dos_headers()->e_magic == IMAGE_DOS_SIGNATURE);
    expect(image->get_nt_headers()->signature == IMAGE_NT_SIGNATURE);
    expect(image->get_file_header() != nullptr);
    expect(image->get_optional_header() != nullptr);
    expect(executable_module.entry_point() == expected_entry_point);
  };

  "wname and system_path match the module path from WinAPI"_test = [] {
    HMODULE kernel32_handle = ::GetModuleHandleW(L"kernel32.dll");
    omni::module kernel32_module = tests::get_loaded_module(kernel32_handle);
    auto kernel32_path = tests::get_module_path(kernel32_handle);

    expect(fatal(kernel32_handle != nullptr));
    expect(fatal(kernel32_module.present()));

    expect(std::wcscmp(kernel32_module.system_path().c_str(), kernel32_path.c_str()) == 0);
    expect(kernel32_module.wname() == kernel32_path.filename().wstring());
  };

  "name and ostream expose the ansi module filename"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    std::string expected_name = version_module.system_path().filename().string();
    std::ostringstream output{};

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));

    output << version_module;

    expect(version_module.name() == expected_name);
    expect(output.str() == expected_name);
  };

  "matches_name_hash accepts stem full name and case variations"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));

    expect(version_module.matches_name_hash(L"version"));
    expect(version_module.matches_name_hash(L"version.dll"));
    expect(version_module.matches_name_hash(L"VERSION.DLL"));
    expect(not version_module.matches_name_hash(L"version.exe"));
    expect(not version_module.matches_name_hash(L"versions"));
  };

  "exports exposes a real export from version.dll"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::module version_module = tests::get_loaded_module(version_dll.handle);
    auto named_exports = version_module.named_exports();
    auto ordinal_exports = version_module.ordinal_exports();
    omni::default_hash export_name{"GetFileVersionInfoSizeW"};
    auto export_entries = tests::get_export_table_entries(version_dll.handle);
    auto* export_entry = find_export_by_name(export_entries, "GetFileVersionInfoSizeW");
    auto named_export_it = named_exports.find(export_name);
    auto ordinal_export_it = ordinal_exports.find(export_entry == nullptr ? 0U : export_entry->ordinal);
    FARPROC export_address = ::GetProcAddress(version_dll.handle, "GetFileVersionInfoSizeW");
    auto export_directory_rva = version_module.image()->get_optional_header()->data_directories.export_directory.rva;
    auto* export_directory = version_module.base_address().ptr<omni::win::export_directory>(export_directory_rva);

    expect(fatal(static_cast<bool>(version_dll)));
    expect(fatal(version_module.present()));
    expect(fatal(export_entry != nullptr));
    expect(fatal(export_address != nullptr));

    expect(named_exports.directory() != nullptr);
    expect(named_exports.directory() == export_directory);
    expect(ordinal_exports.directory() == export_directory);
    expect(named_exports.size() > 0U);
    expect(ordinal_exports.size() >= named_exports.size());
    expect(named_export_it != named_exports.end());
    expect(ordinal_export_it != ordinal_exports.end());
    expect(not named_export_it->is_forwarded());
    expect(not ordinal_export_it->is_forwarded());
    expect(named_export_it->address == export_address);
    expect(ordinal_export_it->address == export_address);
    expect(named_export_it->module_base == version_module.base_address());
    expect(ordinal_export_it->module_base == version_module.base_address());
    expect(ordinal_export_it->ordinal == export_entry->ordinal);
  };
};
