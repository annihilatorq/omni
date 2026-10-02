#include <Windows.h>
#include <psapi.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include "omni/kernel_modules.hpp"
#include "test_utils.hpp"

namespace {

  [[nodiscard]] std::optional<omni::kernel_modules> take_snapshot() {
    auto snapshot = omni::kernel_modules::snapshot();
    if (!snapshot) {
      expect(snapshot.error() == omni::make_error_code(omni::ntstatus::access_denied));
      return std::nullopt;
    }
    return std::move(*snapshot);
  }

  [[nodiscard]] bool is_kernel_image(const omni::kernel_module& module) {
    std::string name{module.name()};
    std::ranges::transform(name, name.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    return name == "ntoskrnl.exe";
  }

  [[nodiscard]] std::optional<std::size_t> current_private_bytes() noexcept {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    if (::GetProcessMemoryInfo(::GetCurrentProcess(),
          reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
          sizeof(counters)) == FALSE) {
      return std::nullopt;
    }
    return counters.PrivateUsage;
  }

} // namespace

ut::suite<"omni::kernel_modules"> kernel_modules_suite = [] {
  "snapshot enumerates the kernel image"_test = [] {
    auto snapshot = take_snapshot();
    if (!snapshot) {
      return;
    }

    expect(fatal(!snapshot->empty()));
    expect(std::ranges::distance(*snapshot) == static_cast<std::ptrdiff_t>(snapshot->size()));
    for (const omni::kernel_module& module : *snapshot) {
      expect(!module.path().empty());
      expect(!module.name().empty());
      expect(module.name().size() <= module.path().size());
      expect(module.path().ends_with(module.name()));
    }

    const auto kernel = std::ranges::find_if(*snapshot, is_kernel_image);
    expect(fatal(kernel != snapshot->end()));
    expect(kernel->size() != 0U);
  };

  "module accessors match native information"_test = [] {
    auto snapshot = take_snapshot();
    if (!snapshot) {
      return;
    }

    for (const omni::kernel_module& module : *snapshot) {
      expect(module.base_address() == module.info().image_base);
      expect(module.size() == module.info().image_size);
      expect(module.flags() == module.info().flags);
      expect(module.load_order_index() == module.info().load_order_index);
      expect(module.load_count() == module.info().load_count);
    }
  };

  "names contain no path separators"_test = [] {
    auto snapshot = take_snapshot();
    if (!snapshot) {
      return;
    }

    for (const omni::kernel_module& module : *snapshot) {
      expect(!module.name().contains('\\'));
      expect(!module.name().contains('/'));
    }
  };

  "begin returns equal iterators on each call"_test = [] {
    auto snapshot = take_snapshot();
    if (!snapshot) {
      return;
    }

    expect(snapshot->begin() == snapshot->begin());
    expect(snapshot->begin() != snapshot->end());
  };

  "two passes enumerate the same paths"_test = [] {
    auto snapshot = take_snapshot();
    if (!snapshot) {
      return;
    }

    const auto paths = std::views::transform([](const omni::kernel_module& module) { return module.path(); });
    expect(std::ranges::equal(*snapshot | paths, *snapshot | paths));
  };

  "works with views::filter and ranges::find_if"_test = [] {
    auto snapshot = take_snapshot();
    if (!snapshot) {
      return;
    }

    auto named_modules =
      *snapshot | std::views::filter([](const omni::kernel_module& module) { return !module.name().empty(); });
    expect(std::ranges::distance(named_modules) == static_cast<std::ptrdiff_t>(snapshot->size()));
    expect(std::ranges::find_if(*snapshot, is_kernel_image) != snapshot->end());
  };

  "moved-from snapshot is empty and moved-to iterates"_test = [] {
    auto source = take_snapshot();
    if (!source) {
      return;
    }

    omni::kernel_modules target = std::move(*source);
    expect(source->begin() == source->end());
    expect(source->empty());
    expect(source->size() == 0U);
    expect(target.begin() != target.end());
    expect(std::ranges::find_if(target, is_kernel_image) != target.end());
  };

  "move assignment replaces the target and empties the source"_test = [] {
    auto source = take_snapshot();
    auto target = take_snapshot();
    if (!source || !target) {
      return;
    }

    const std::size_t source_size = source->size();
    *target = std::move(*source);

    expect(source->begin() == source->end());
    expect(source->empty());
    expect(source->size() == 0U);
    expect(target->size() == source_size);
    expect(target->begin() != target->end());
    expect(std::ranges::find_if(*target, is_kernel_image) != target->end());
  };

  "operator-> and operator* refer to the same module"_test = [] {
    auto snapshot = take_snapshot();
    if (!snapshot) {
      return;
    }

    const auto it = snapshot->begin();
    expect(it->path() == (*it).path());
    expect(it->name() == (*it).name());
    expect(it->size() == (*it).size());
    expect(&it->info() == &(*it).info());
  };

  "post-increment returns the previous position"_test = [] {
    auto snapshot = take_snapshot();
    if (!snapshot) {
      return;
    }

    expect(fatal(snapshot->size() > 1U));
    auto it = snapshot->begin();
    const auto previous = it++;
    expect(previous == snapshot->begin());
    expect(it != snapshot->begin());
    expect(previous->path() == snapshot->begin()->path());
  };

  "repeated snapshots do not leak memory"_test = [] {
    auto initial = take_snapshot();
    if (!initial) {
      return;
    }
    initial.reset();

    constexpr int warmup_iterations = 10;
    constexpr int measured_iterations = 1000;
    constexpr int measurement_attempts = 3;
    constexpr std::size_t memory_tolerance = 1U << 20U;

    const auto take_successful_snapshot = [] {
      expect(fatal(omni::kernel_modules::snapshot().has_value()));
    };

    for (int i = 0; i < warmup_iterations; ++i) {
      take_successful_snapshot();
    }

    bool stable_memory_usage = false;
    for (int attempt = 0; attempt < measurement_attempts; ++attempt) {
      const auto bytes_before = current_private_bytes();
      expect(fatal(bytes_before.has_value()));

      for (int i = 0; i < measured_iterations; ++i) {
        take_successful_snapshot();
      }

      const auto bytes_after = current_private_bytes();
      expect(fatal(bytes_after.has_value()));
      if (*bytes_after <= *bytes_before + memory_tolerance) {
        stable_memory_usage = true;
        break;
      }
    }

    expect(stable_memory_usage) << "virtual_free must release the buffer instead of growing private memory every batch";
  };
};
