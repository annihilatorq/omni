#include <Windows.h>

#include <filesystem>
#include <set>
#include <utility>
#include <vector>

#include "omni/modules.hpp"
#include "test_utils.hpp"

namespace tests = omni::tests;

ut::suite<"omni::modules"> modules_suite = [] {
  "begin points at the process image"_test = [] {
    omni::modules loaded_modules{};
    HMODULE executable_handle = ::GetModuleHandleW(nullptr);
    auto first = loaded_modules.begin();
    auto executable_path = tests::get_module_path(executable_handle);
    auto first_path = first->system_path();

    expect(executable_handle != nullptr);
    expect(first != loaded_modules.end());
    expect(first->present());
    expect(first->base_address() == executable_handle);
    expect(first->native_handle() == executable_handle);
    expect(not first_path.empty());
    expect(not first_path.filename().wstring().empty());
    expect(first_path == executable_path);
  };

  "base_module points at the process image"_test = [] {
    HMODULE executable_handle = ::GetModuleHandleW(nullptr);
    auto executable_path = tests::get_module_path(executable_handle);

    omni::module base_module = omni::base_module();
    auto base_module_path = base_module.system_path();

    expect(executable_handle != nullptr);
    expect(base_module.present());
    expect(base_module.base_address() == executable_handle);
    expect(base_module.native_handle() == executable_handle);
    expect(not base_module_path.empty());
    expect(not base_module_path.filename().wstring().empty());
    expect(base_module_path == executable_path);
  };

  "iteration yields unique present modules"_test = [] {
    std::vector<omni::address> module_bases{};

    for (const omni::module& module : omni::modules{}) {
      expect(module.present());
      expect(module.base_address() != nullptr);
      expect(not module.wname().empty());
      module_bases.push_back(module.base_address());
    }

    std::set<omni::address> unique_bases{std::from_range, module_bases};

    expect(module_bases.size() >= 3U);
    expect(unique_bases.size() == module_bases.size());
  };

  "skip advances the range begin like iterator increment"_test = [] {
    omni::modules loaded_modules{};
    auto second = loaded_modules.begin();

    expect(second != loaded_modules.end());
    expect(++second != loaded_modules.end());

    omni::modules skipped_modules{};
    auto skipped = skipped_modules.skip().begin();

    expect(skipped != skipped_modules.end());
    expect(skipped->base_address() == second->base_address());
    expect(skipped->wname() == second->wname());
  };

  "find contains and find_if locate kernel32"_test = [] {
    HMODULE kernel32_handle = ::GetModuleHandleW(L"kernel32.dll");
    omni::address kernel32_address{kernel32_handle};
    omni::default_hash kernel32_hash{L"kernel32"};
    omni::default_hash kernel32_dll_hash{L"kernel32.dll"};

    expect(fatal(kernel32_handle != nullptr));

    omni::modules loaded_modules{};
    auto by_address = loaded_modules.find(kernel32_address);
    auto by_name = loaded_modules.find(kernel32_hash);
    auto by_predicate = loaded_modules.find_if(
      [kernel32_address](const omni::module& module) { return module.base_address() == kernel32_address; });

    expect(loaded_modules.contains(kernel32_address));
    expect(loaded_modules.contains(kernel32_hash));
    expect(loaded_modules.contains(kernel32_dll_hash));

    expect(by_address != loaded_modules.end());
    expect(by_name != loaded_modules.end());
    expect(by_predicate != loaded_modules.end());

    expect(by_address->base_address() == kernel32_address);
    expect(by_name->base_address() == kernel32_address);
    expect(by_predicate->base_address() == kernel32_address);
  };

  "bidirectional iteration reaches the last loaded module"_test = [] {
    omni::modules loaded_modules{};
    auto last = loaded_modules.begin();
    auto it = loaded_modules.begin();

    expect(last != loaded_modules.end());

    while (it != loaded_modules.end()) {
      std::ignore = last = it;
      std::advance(it, 1);
    }

    auto from_end = loaded_modules.end();
    std::advance(from_end, -1);

    expect(from_end->present());
    expect(from_end->base_address() == last->base_address());
    expect(from_end->wname() == last->wname());
  };

  "iteration sees modules loaded through WinAPI"_test = [] {
    tests::loaded_library version_dll{L"version.dll"};
    omni::default_hash version_hash{L"version"};
    omni::default_hash version_dll_hash{L"version.dll"};

    expect(static_cast<bool>(version_dll));

    omni::modules loaded_modules{};
    auto version_it = loaded_modules.find(omni::address{version_dll.handle});

    expect(version_it != loaded_modules.end());
    expect(version_it->base_address() == version_dll.handle);
    expect(version_it->matches_name_hash(version_hash));
    expect(version_it->matches_name_hash(version_dll_hash));
  };
};
