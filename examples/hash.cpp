#include "omni/hash.hpp"
#include "omni/concepts/concepts.hpp"

#include <array>
#include <cstdint>
#include <print>
#include <ranges>
#include <span>
#include <string_view>
#include <utility>

namespace {

  template <std::unsigned_integral T>
  class djb2_hash {
    constexpr static auto initial_value = T{5381};

   public:
    using value_type = T;

    constexpr djb2_hash() = default;

    constexpr explicit(false) djb2_hash(value_type value): value_(value) {}

    template <typename CharT, std::size_t N>
    consteval explicit(false) djb2_hash(const CharT (&string)[N]) {
      for (std::size_t i{}; i < N - 1; i++) {
        value_ = append(value_, string[i]);
      }
    }

    consteval explicit(false) djb2_hash(omni::concepts::hashable auto string) {
      for (std::size_t i{}; i < string.size(); i++) {
        value_ = append(value_, string[i]);
      }
    }

    [[nodiscard]] value_type operator()(omni::concepts::hashable auto object) {
      auto value = initial_value;
      for (std::size_t i{}; i < object.size(); i++) {
        value = append(value, object[i]);
      }
      return value;
    }

    [[nodiscard]] value_type value() const {
      return value_;
    }

    [[nodiscard]] constexpr bool operator==(value_type other) const {
      return value_ == other;
    }

    [[nodiscard]] constexpr bool operator==(const djb2_hash& other) const {
      return value_ == other.value_;
    }

   private:
    template <typename CharT>
    [[nodiscard]] constexpr static value_type append(value_type accumulator, CharT character) {
      return (accumulator * 33) + static_cast<value_type>(to_lower(character));
    }

    template <typename CharT>
    [[nodiscard]] constexpr static CharT to_lower(CharT character) {
      return ((character >= 'A' && character <= 'Z') ? (character + 32) : character);
    }

    value_type value_{initial_value};
  };

  using djb2_64 = djb2_hash<std::uint64_t>;

  static_assert(omni::concepts::hash<djb2_64>);

  template <omni::concepts::hash Hasher>
  void print_hashes(std::string_view label, std::span<const std::string_view> names) {
    std::println("{}:", label);
    for (std::string_view name : names) {
      std::println("  {:<28} -> {}", name, omni::hash<Hasher>(name));
    }
  }

  [[nodiscard]] bool is_nt_name(std::string_view name) {
    return name.starts_with("Nt");
  }

  template <omni::concepts::hash Hasher>
  [[nodiscard]] std::pair<std::string_view, typename Hasher::value_type> make_named_hash(std::string_view name) {
    return {name, omni::hash<Hasher>(name)};
  }

} // namespace

int main() {
  constexpr omni::fnv1a32 nt_query_information_process32{"NtQueryInformationProcess"};
  constexpr omni::fnv1a64 nt_query_information_process64{"NtQueryInformationProcess"};
  constexpr djb2_64 nt_query_information_process_djb2{"NtQueryInformationProcess"};
  constexpr omni::hash_pair<> version_lookup{"GetFileVersionInfoSizeW", "version.dll"};
  constexpr omni::hash_pair<djb2_64> module_lookup{"GetModuleHandleW", "kernel32.dll"};

  constexpr std::array<std::string_view, 4> names{
    "kernel32.dll",
    "KERNEL32.DLL",
    "NtQueryInformationProcess",
    "GetCurrentProcessId",
  };

  std::println("Compile-time hashes are plain strongly-typed values:");
  std::println(R"(  fnv1a32("NtQueryInformationProcess") = {})", nt_query_information_process32);
  std::println(R"(  fnv1a64("NtQueryInformationProcess") = {})", nt_query_information_process64);
  std::println(R"(  djb2_64("NtQueryInformationProcess") = {})", nt_query_information_process_djb2.value());
  std::println("  hash_pair(version lookup) = ({}, {})", version_lookup.first, version_lookup.second);
  std::println("  hash_pair<djb2_64>(module lookup) = ({}, {})", module_lookup.first.value(), module_lookup.second.value());

  std::println();

  std::println("Hashes are ASCII case-insensitive, which is handy for Windows loader names:");
  std::println("  kernel32.dll == KERNEL32.DLL -> {}", omni::fnv1a64{"kernel32.dll"} == omni::fnv1a64{"KERNEL32.DLL"});
  std::println(R"(  djb2_64("kernel32.dll") == djb2_64("KERNEL32.DLL") -> {})",
    djb2_64{"kernel32.dll"} == djb2_64{"KERNEL32.DLL"});

  std::println();

  print_hashes<omni::fnv1a32>("Runtime fnv1a32", names);
  std::println();

  print_hashes<omni::fnv1a64>("Runtime fnv1a64", names);
  std::println();

  print_hashes<djb2_64>("Runtime djb2_64", names);
  std::println();

  std::println("Custom hashers plug into the same ranges pipeline:");
  auto nt_hashes = names | std::views::filter(is_nt_name) | std::views::transform(make_named_hash<djb2_64>);
  for (auto [name, hash] : nt_hashes) {
    std::println("  {:<28} -> {}", name, hash);
  }
}
