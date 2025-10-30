#include "shadowsyscall.hpp"

template <typename... Args>
void debug_log(const std::format_string<Args...> fmt, Args&&... args) {
  std::cout << std::format(fmt, std::forward<Args>(args)...) << '\n';
}

template <typename... Args>
void log_value(std::string_view label, Args&&... args) {
  constexpr int column_width = 32;
  std::cout << std::format("{:>{}}: ", label, column_width);
  if constexpr (sizeof...(Args) > 0) {
    (std::cout << ... << std::forward<Args>(args));
  }
  std::cout << '\n';
}

void new_line(int count = 1) {
  for (int i = 0; i < count; i++)
    std::cout.put('\n');
}

#define obfuscate_string(str)    \
  []() {                         \
    /* Any string obfuscation */ \
    return str;                  \
  }

int main() {
  // Enumerate every dll loaded into current process
  const auto dlls = shadow::dlls();
  debug_log("List of DLLs loaded in the current process:");
  for (const auto& dll : shadow::dlls())
    debug_log("{} : {}", dll.filepath().string(), dll.native_handle());

  new_line();

  // Find a specific DLL loaded into the current process.
  // "ntdll.dll" doesn't leave the string in the executable -
  // it’s hashed at compile time (consteval guarantee)
  // The implementation doesn't care about the ".dll" suffix

  // after compilation it will become 384989384324938
  auto ntdll_name_hash = shadow::hash64_t{"ntdll"};
  auto ntdll = shadow::dll(ntdll_name_hash);

  auto base_module = shadow::base_module();
  debug_log("Current .exe filepath: {}", base_module.filepath().string());
  debug_log("Current .text section checksum: {}",
            base_module.section_checksum<std::size_t>(".text"));
  debug_log("Current module handle: {}",
            base_module.native_handle());  // same as GetModuleHandle(nullptr)
  new_line();

  auto image = ntdll.image();
  auto sections = image->get_nt_headers()->sections();
  auto optional_header = image->get_optional_header();
  auto exports = ntdll.exports();

  constexpr int export_entries_count = 5;
  const auto first_n_exports = exports | std::views::take(export_entries_count);

  debug_log("{} first exports of ntdll.dll", export_entries_count);
  for (const shadow::win::export_t& exp : first_n_exports) {
    const auto& [name, address, ordinal] = std::make_tuple(exp.name, exp.address, exp.ordinal);
    debug_log("{} : {} : {}", name, address.ptr(), ordinal);
  }

  new_line();

  auto it = exports.find_if([](const shadow::win::export_t& export_data) -> bool {
    // after compilation becomes 384989384324938
    constexpr auto compiletime_hash = shadow::hash64_t{"NtQuerySystemInformation"};
    // operator() (called in runtime) accepts any range that have access by index
    const auto runtime_hash = shadow::hash64_t{}(export_data.name);
    return compiletime_hash == runtime_hash;
  });

  if (it == exports.end()) {
    debug_log("Failed to find NtQuerySystemInformation export");
    return 1;
  }

  const auto& export_data = *it;
  debug_log("Export {} VA is {}", export_data.name, export_data.address.ptr());

  constexpr int ordinal = 10;
  const auto& ordinal_export =
      shadow::exported_symbol(shadow::use_ordinal, ntdll_name_hash, ordinal);
  debug_log("Export on ordinal {} in ntdll.dll is presented on VA {}", ordinal,
            ordinal_export.address().ptr());

  // "location" returns a DLL struct that contains this export
  std::filesystem::path dll_path = shadow::exported_symbol("Sleep").location().name().to_path();
  debug_log("The DLL that contains the Sleep export is: {}", dll_path.string());

  new_line(2);

  log_value("[NTDLL]");
  log_value("Base Address", ntdll.base_address().ptr());
  log_value("Native Handle", ntdll.native_handle());
  log_value("Entry Point", ntdll.entry_point());
  log_value("Name", ntdll.name().string());
  log_value("Path to File", ntdll.filepath().to_path());
  log_value("Reference count", ntdll.reference_count());
  log_value("Image Size", optional_header->size_image);
  log_value("Sections count", std::ranges::size(sections));
  log_value("Exports count", exports.size());
  new_line();

  // shared_data parses KUSER_SHARED_DATA
  // The class is a high-level wrapper for parsing,
  // which saves you from pointer arithmetic
  auto shared = shadow::shared_data();

  // shared_data() implements the most popular getters, while you
  // can access the entire KUSER_SHARED_DATA structure as follows:
  auto kuser_shared_data = shared.get();

  // This structure weighs 3528 bytes (on x64 architecture), so for
  // obvious reasons I will not display all its fields in this example
  std::ignore = kuser_shared_data->system_expiration_date;

  log_value("[KERNEL]");
  log_value("Safe boot", shared.safe_boot_enabled());
  log_value("Boot ID", shared.boot_id());
  log_value("Physical Pages Num", shared.physical_pages_num());
  log_value("Kernel debugger present", shared.kernel_debugger_present());
  log_value("System root", shared.system_root().to_path().string());
  new_line();

  auto system = shared.system();

  log_value("[SYSTEM]");
  log_value("Windows 11", system.is_windows_11());
  log_value("Windows 10", system.is_windows_10());
  log_value("Windows 7", system.is_windows_7());
  log_value("Windows XP", system.is_windows_xp());
  log_value("Windows Vista", system.is_windows_vista());
  log_value("OS Major Version", system.major_version());
  log_value("OS Minor Version", system.minor_version());
  log_value("OS Build Number", system.build_number());
  log_value("Formatted OS String", system.formatted());
  new_line();

  auto unix_timestamp = shared.unix_epoch_timestamp();
  auto windows_timestamp = shared.windows_epoch_timestamp();

  log_value("[TIME]");
  log_value("Unix Time", unix_timestamp.utc().time_since_epoch());
  log_value("Unix Time", unix_timestamp.utc().format_iso8601());
  log_value("Unix Time (Local)", unix_timestamp.local().time_since_epoch());
  log_value("Unix Time (Local) (ISO 8601)", unix_timestamp.local().format_iso8601());
  log_value("Windows Time", shared.windows_epoch_timestamp());
  log_value("Timezone ID", shared.timezone_id());
  log_value("Timezone offset", shared.timezone_offset<std::chrono::seconds>());

  // Iterators are compatible with the ranges library
  static_assert(std::bidirectional_iterator<shadow::detail::export_view::iterator>);
  static_assert(std::bidirectional_iterator<shadow::detail::module_view::iterator>);

  // Code below DOES NOT COMPILE. hash*_t requires a string literal
  // because the hashing of the string happens at compile time, so there
  // is no point in you obfuscating the string in any way, because it
  // will turn into a number and disappear from the binary after compilation.
  //
  // constexpr auto hash_that_causes_ct_error = shadow::hash64_t{string_obfuscator("string")};
  //
  // The right way to do it is:
  // constexpr auto valid_hash = shadow::hash64_t{"string"};
}