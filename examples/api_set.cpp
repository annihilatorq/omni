#include "omni/api_sets.hpp"

#include <algorithm>
#include <print>
#include <ranges>
#include <string>

namespace {

  [[nodiscard]] std::string narrow_ascii(std::wstring_view value) {
    return value | std::views::transform([](wchar_t ch) { return static_cast<char>(ch); }) | std::ranges::to<std::string>();
  }

  [[nodiscard]] bool has_default_host(const omni::api_set& api_set) {
    auto default_host = api_set.default_host();
    return default_host.has_value() && !default_host->value.empty();
  }

} // namespace

int main() {
  omni::api_sets api_sets{};

  auto api_set = api_sets.find_if([](const omni::api_set& candidate) {
    if (!has_default_host(candidate)) {
      return false;
    }

    return std::ranges::any_of(candidate.hosts(),
      [](const omni::api_set_host& host) { return !host.alias.empty() && !host.value.empty(); });
  });

  if (api_set == api_sets.end()) {
    api_set = api_sets.find_if(has_default_host);
  }

  if (api_set == api_sets.end()) {
    std::println("No implemented API set was found.");
    return 1;
  }

  auto hosts = api_set->hosts();
  auto default_host = api_set->resolve_host();
  auto alias_host =
    std::ranges::find_if(hosts, [](const omni::api_set_host& host) { return !host.alias.empty() && !host.value.empty(); });

  std::println("API sets are small value objects backed by the live process schema:");
  std::println("  contract             : {}", narrow_ascii(api_set->contract_name()));
  std::println("  sealed               : {}", api_set->is_sealed());
  std::println("  host count           : {}", hosts.size());
  std::println("  default host         : {}", default_host.has_value() ? narrow_ascii(default_host->value) : "<none>");

  std::println();
  std::println("The host table is a regular range:");
  for (const auto& host : hosts | std::views::take(5)) {
    auto selector = host.alias.empty() ? std::string{"<default>"} : narrow_ascii(host.alias);
    auto target = host.value.empty() ? std::string{"<unimplemented>"} : narrow_ascii(host.value);
    std::println("  {} -> {}", selector, target);
  }

  auto fallback_host = api_set->resolve_host(L"omni-demo-alias");

  std::println();
  std::println("Convenience helpers apply the schema rules for you:");
  std::println("  resolve_host()       : {}", default_host.has_value() ? narrow_ascii(default_host->value) : "<none>");
  std::println("  resolve_host(miss)   : {}", fallback_host.has_value() ? narrow_ascii(fallback_host->value) : "<none>");

  if (alias_host != hosts.end()) {
    auto resolved_alias = api_set->resolve_host(alias_host->alias);

    std::println();
    std::println("Alias-specific remaps are one call away:");
    std::println("  alias                : {}", narrow_ascii(alias_host->alias));
    std::println("  resolve_host(alias)  : {}", resolved_alias.has_value() ? narrow_ascii(resolved_alias->value) : "<none>");
  }
}
