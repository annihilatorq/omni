#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <string>

#include "omni/kernel_modules.hpp"
#include "test_utils.hpp"

ut::suite<"omni::kernel_modules"> kernel_modules_suite = [] {
  "snapshot enumerates the kernel image"_test = [] {
    auto snapshot = omni::kernel_modules::snapshot();

    if (!snapshot) {
      // Recent Windows builds may deny this query to non-elevated callers.
      expect(snapshot.error() == omni::make_error_code(omni::ntstatus::access_denied));
      return;
    }

    expect(fatal(!snapshot->empty()));
    expect(std::ranges::distance(*snapshot) == static_cast<std::ptrdiff_t>(snapshot->size()));

    bool found_kernel{};
    for (const omni::kernel_module& module : *snapshot) {
      expect(!module.path().empty());
      expect(!module.name().empty());
      expect(module.name().size() <= module.path().size());
      expect(module.path().ends_with(module.name()));

      std::string name{module.name()};
      std::ranges::transform(name, name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
      if (name == "ntoskrnl.exe") {
        found_kernel = true;
        expect(module.size() != 0U);
      }
    }

    expect(found_kernel);
  };
};
