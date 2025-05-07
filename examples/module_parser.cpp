#include <iostream>
#include <string>
#include "shadowsyscall.hpp"

#define obfuscate_string(str)    \
  []() {                         \
    /* Any string obfuscation */ \
    return str;                  \
  }

int main() {
  // Enumerate every dll loaded into current process
  for (const auto& dll : shadow::dlls())
    std::cout << dll.filepath().string() << " : " << dll.native_handle() << "\n";

  std::cout.put('\n');

  // Find a specific DLL loaded into the current process.
  // "ntdll.dll" doesn't leave the string in the executable -
  // it’s hashed at compile time (consteval guarantee)
  // The implementation doesn't care about the ".dll" suffix

  auto ntdll = shadow::dll("ntdll" /* after compilation it will become 384989384324938 */);

  auto current_module = shadow::current_module();
  std::cout << "Current .exe filepath: " << current_module.filepath().string() << "\n";
  std::cout << "Current .text section checksum: "
            << current_module.section_checksum<std::size_t>(".text") << "\n\n";

  auto image = ntdll.image();
  auto nt_headers = image->get_nt_headers();
  auto optional_header = image->get_optional_header();

  std::cout << ntdll.base_address().ptr() << '\n';  // .base_address() returns address_t
  std::cout << ntdll.native_handle() << '\n';       // .native_handle() returns void*
  std::cout << ntdll.entry_point() << '\n';         // .entry_point() returns address_t, if present
  std::cout << ntdll.name().string() << '\n';       // .name() returns win::unicode_string
  std::cout << ntdll.filepath().to_path() << '\n';  // .filepath() returns win::unicode_string
  std::cout << nt_headers->signature << '\n';       // returns uint32_t, NT magic value
  std::cout << optional_header->size_image << "\n\n";  // returns uint32_t, loaded NTDLL image size

  constexpr int export_entries_count = 5;
  const auto exports = ntdll.exports() | std::views::take(export_entries_count);

  std::cout << export_entries_count << " exports of ntdll.dll:\n";
  for (const shadow::win::export_t& exp : exports) {
    const auto& [name, address, ordinal] = std::make_tuple(exp.name, exp.address, exp.ordinal);
    std::cout << name << " : " << address << " : " << ordinal << '\n';
  }

  std::cout.put('\n');

  auto it = ntdll.exports().find_if([](const shadow::win::export_t& export_data) -> bool {
    // after compilation becomes 384989384324938
    constexpr auto compiletime_hash = shadow::hash64_t{"NtQuerySystemInformation"};
    // operator() (called in runtime) accepts any range that have access by index
    const auto runtime_hash = shadow::hash64_t{}(export_data.name);
    return compiletime_hash == runtime_hash;
  });

  const shadow::win::export_t& export_data = *it;
  std::cout << "Found target export:\n"
            << export_data.name << " : " << export_data.address << "\n\n";

  // "location" returns a DLL struct that contains this export
  std::cout << "The DLL that contains the Sleep export is: "
            << shadow::exported_symbol("Sleep").location().name().to_path() << "\n\n";

  // shared_data parses KUSER_SHARED_DATA
  // The class is a high-level wrapper for parsing,
  // which saves you from pointer arithmetic

  auto shared = shadow::shared_data();

  std::cout << shared.safe_boot_enabled() << '\n';
  std::cout << shared.boot_id() << '\n';
  std::cout << shared.physical_pages_num() << '\n';
  std::cout << shared.kernel_debugger_present() << '\n';
  std::wcout << shared.system_root().to_path() << '\n';

  std::cout << shared.system().is_windows_11() << '\n';
  std::cout << shared.system().is_windows_10() << '\n';
  std::cout << shared.system().is_windows_7() << '\n';
  std::cout << shared.system().build_number() << '\n';
  std::cout << shared.system().formatted() << '\n';

  std::cout << shared.unix_epoch_timestamp().utc().time_since_epoch() << '\n';
  std::cout << shared.unix_epoch_timestamp().utc().format_iso8601() << '\n';
  std::cout << shared.unix_epoch_timestamp().local().time_since_epoch() << '\n';
  std::cout << shared.unix_epoch_timestamp().local().format_iso8601() << '\n';
  std::cout << shared.timezone_offset<std::chrono::seconds>() << "\n\n";

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