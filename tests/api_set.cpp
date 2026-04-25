#include <algorithm>
#include <string>

#include "omni/api_sets.hpp"
#include "test_utils.hpp"

namespace tests = omni::tests;

namespace {

  [[nodiscard]] bool equal_module_name(std::wstring_view lhs, std::wstring_view rhs) {
    return omni::hash<omni::default_hash>(lhs) == omni::hash<omni::default_hash>(rhs);
  }

} // namespace

ut::suite<"omni::api_set"> api_set_suite = [] {
  "default constructed api_set is empty"_test = [] {
    omni::api_set api_set{};
    omni::api_set_hosts hosts = api_set.hosts();

    expect(api_set.contract_name().empty());
    expect(not api_set.is_sealed());
    expect(hosts.size() == 0U);
    expect(hosts.begin() == hosts.end());
    expect(not api_set.default_host().has_value());
    expect(not api_set.resolve_host().has_value());
  };

  "schema-resolved default host matches Windows when the raw schema provides one"_test = [] {
    tests::api_set_query_api api_query{};
    omni::api_sets api_sets{};

    auto implemented_api_set = api_sets.find_if([&api_query](const omni::api_set& api_set) {
      std::string contract_name = tests::narrow_ascii(api_set.contract_name());
      if (api_query.is_api_set_implemented(contract_name.c_str()) == FALSE) {
        return false;
      }

      auto default_host = api_set.default_host();
      return default_host.has_value() && !default_host->value.empty();
    });

    expect(fatal(static_cast<bool>(api_query)));
    expect(fatal(implemented_api_set != api_sets.end()));

    auto module_base_name =
      tests::query_api_set_module_base_name(api_query, tests::narrow_ascii(implemented_api_set->contract_name()));
    auto default_host = implemented_api_set->default_host();
    auto resolved_host = implemented_api_set->resolve_host();

    expect(SUCCEEDED(module_base_name.hr));
    expect(not module_base_name.module_base_name.empty());
    expect(fatal(default_host.has_value()));
    expect(fatal(resolved_host.has_value()));
    expect(equal_module_name(default_host->value, module_base_name.module_base_name));
    expect(equal_module_name(resolved_host->value, module_base_name.module_base_name));
  };

  "alias-specific remaps override the default host inside the raw schema"_test = [] {
    omni::api_sets api_sets{};

    auto aliased_api_set = api_sets.find_if([](const omni::api_set& api_set) {
      auto default_host = api_set.default_host();
      if (!default_host.has_value() || default_host->value.empty()) {
        return false;
      }

      return std::ranges::any_of(api_set.hosts(),
        [](const omni::api_set_host& host) { return !host.alias.empty() && !host.value.empty(); });
    });

    expect(fatal(aliased_api_set != api_sets.end()));

    auto default_host = aliased_api_set->default_host();
    auto hosts = aliased_api_set->hosts();
    auto alias_host =
      std::ranges::find_if(hosts, [](const omni::api_set_host& host) { return !host.alias.empty() && !host.value.empty(); });

    expect(fatal(default_host.has_value()));
    expect(fatal(alias_host != hosts.end()));
    auto resolved_host = aliased_api_set->resolve_host(alias_host->alias);
    auto resolved_default_host = aliased_api_set->resolve_host();
    expect(fatal(resolved_host.has_value()));
    expect(fatal(resolved_default_host.has_value()));
    expect(resolved_host->alias == alias_host->alias);
    expect(resolved_host->value == alias_host->value);
    expect(resolved_default_host->value == default_host->value);
  };
};
