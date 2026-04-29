#include <limits>
#include <utility>

#include "omni/hash.hpp"
#include "omni/module_export.hpp"
#include "omni/modules.hpp"
#include "omni/status.hpp"
#include "test_utils.hpp"

ut::suite<"std::format"> std_format_suite = [] {
  "formats initialized and uninitialized omni::module"_test = [] {
    omni::module empty_module;
    omni::module ntdll = omni::get_module("ntdll.dll");

    expect(std::format("{}", empty_module).empty());
    expect(std::format("{}", ntdll) == ntdll.name());
  };

  "formats initialized and uninitialized named and ordinal exports"_test = [] {
    auto kernel32 = omni::get_module("kernel32.dll");

    omni::named_export empty_named_export;
    omni::ordinal_export empty_ordinal_export;
    auto nt_alloc = omni::get_export("NtAllocateVirtualMemory", kernel32);
    omni::ordinal_export first_ordinal_export;

    // Find first ordinal-only export across all modules
    for (const auto& module : omni::modules{}) {
      auto ordinal_exports = module.ordinal_exports();
      if (auto it = ordinal_exports.begin(); it != ordinal_exports.end()) {
        first_ordinal_export = *it;
        break;
      }
    }

    expect(std::format("{}", empty_named_export).empty());
    expect(std::format("{}", nt_alloc) == nt_alloc.name);
    expect(std::format("{}", empty_ordinal_export).empty());
    expect(std::format("{}", first_ordinal_export) == std::format("#{}", first_ordinal_export.ordinal));
  };

  "formats omni::address"_test = [] {
    constexpr auto max_uintptr_t = (std::numeric_limits<std::uintptr_t>::max)();

    expect(std::format("{}", omni::address{}) == "0");
    expect(std::format("{}", omni::address{1000}) == "1000");
    expect(std::format("{}", omni::address{max_uintptr_t}) == std::to_string(max_uintptr_t));
  };

  "formats omni::ntstatus"_test = [] {
    expect(std::format("{}", omni::ntstatus::access_violation) ==
           std::to_string(std::to_underlying(omni::ntstatus::access_violation)));
  };

  "formats omni::status"_test = [] {
    expect(std::format("{}", omni::status{}) == "0");
    expect(std::format("{}", omni::status{static_cast<std::int32_t>(0xC0000005)}) == "-1073741819");
  };

  "formats omni hash types"_test = [] {
    constexpr auto hash32 = omni::fnv1a32{"hello omni"};
    constexpr auto hash64 = omni::fnv1a64{"hello omni"};
    constexpr auto default_hash = omni::default_hash{"hello omni"};

    expect(std::format("{}", omni::fnv1a32{}) == std::to_string(omni::fnv1a32::initial_value));
    expect(std::format("{}", hash32) == std::to_string(hash32.value()));

    expect(std::format("{}", omni::fnv1a64{}) == std::to_string(omni::fnv1a64::initial_value));
    expect(std::format("{}", hash64) == std::to_string(hash64.value()));

    expect(std::format("{}", omni::default_hash{}) == std::to_string(omni::default_hash::initial_value));
    expect(std::format("{}", default_hash) == std::to_string(default_hash.value()));
  };
};
