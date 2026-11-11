#include <iostream>
#include <string>
#include <string_view>

#include "omni/api_sets.hpp"
#include "test_utils.hpp"

namespace tests = omni::tests;

namespace {

  [[nodiscard]] bool equal_module_name(std::wstring_view lhs, std::wstring_view rhs) {
    return omni::hash<omni::default_hash>(lhs) == omni::hash<omni::default_hash>(rhs);
  }

  [[nodiscard]] bool has_non_empty_default_host(const omni::api_set& api_set) {
    auto default_host = api_set.default_host();
    return default_host.has_value() && !default_host->value.empty();
  }

  [[nodiscard]] bool consume_trailing_number(std::wstring_view& text) {
    auto last_non_digit = text.find_last_not_of(L"0123456789");
    auto digits_begin = last_non_digit == std::wstring_view::npos ? 0U : last_non_digit + 1;
    if (digits_begin == text.size()) {
      return false;
    }

    text.remove_suffix(text.size() - digits_begin);
    return true;
  }

  [[nodiscard]] std::size_t find_contract_version_suffix(std::wstring_view contract_name) {
    for (int segment = 0; segment < 3; ++segment) {
      if (!consume_trailing_number(contract_name)) {
        return std::wstring_view::npos;
      }

      if (segment < 2) {
        if (!contract_name.ends_with(L'-')) {
          return std::wstring_view::npos;
        }
        contract_name.remove_suffix(1);
      }
    }

    if (!contract_name.ends_with(L"-l")) {
      return std::wstring_view::npos;
    }

    return contract_name.size() - 2;
  }

} // namespace

int main() {
  "public api set contracts follow the official naming convention"_test = [] {
    std::size_t entry_count{};
    std::size_t public_contract_count{};

    for (const omni::api_set& api_set : omni::api_sets{}) {
      std::wstring_view contract_name = api_set.contract_name();
      std::string contract_name_ascii = tests::narrow_ascii(contract_name);

      expect(!contract_name.empty()) << "empty namespace entry at index " << entry_count;

      const bool is_public_contract = contract_name.starts_with(L"api-") || contract_name.starts_with(L"ext-");

      if (is_public_contract) {
        expect(find_contract_version_suffix(contract_name) != std::wstring_view::npos)
          << "invalid public api-set contract name at index " << entry_count << ", contract=[" << contract_name_ascii << "]";

        ++public_contract_count;
      }

      ++entry_count;
    }

    expect(entry_count > 0U) << "api set enumeration returned no entries";
    expect(public_contract_count > 0U) << "api set enumeration returned no public contracts";
  };

  "schema-resolved default hosts match Windows when the raw schema provides one"_test = [] {
    tests::api_set_query_api api_query{};
    constexpr std::size_t max_fallback_checks = 200;
    std::size_t checked_contracts{};
    std::size_t fallback_checks{};

    require(static_cast<bool>(api_query));

    for (const omni::api_set& api_set : omni::api_sets{}) {
      if (fallback_checks >= max_fallback_checks) {
        break;
      }

      auto default_host = api_set.default_host();
      if (!default_host.has_value() || default_host->value.empty()) {
        continue;
      }

      std::string contract_name = tests::narrow_ascii(api_set.contract_name());
      if (api_query.is_api_set_implemented(contract_name.c_str()) == FALSE) {
        continue;
      }

      auto module_base_name = tests::query_api_set_module_base_name(api_query, contract_name);
      fallback_checks += module_base_name.used_fallback;

      if (FAILED(module_base_name.hr)) {
        continue;
      }

      auto resolved_host = api_set.resolve_host();
      expect(equal_module_name(default_host->value, module_base_name.module_base_name) && resolved_host.has_value() &&
             equal_module_name(resolved_host->value, module_base_name.module_base_name))
        << "host mismatch, contract=[" << contract_name << "]";

      ++checked_contracts;
    }

    if (fallback_checks != 0U) {
      std::cerr << "[omni::api_sets] " << fallback_checks << " contracts resolved via LoadLibrary fallback (limit "
                << max_fallback_checks << ")\n";
    }

    expect(checked_contracts > 0U);
  };

  "contracts without an implementation are reported unavailable by Windows"_test = [] {
    tests::api_set_query_api api_query{};
    omni::api_sets api_sets{};

    require(static_cast<bool>(api_query));

    auto unavailable_api_set = api_sets.find_if([&api_query](const omni::api_set& api_set) {
      if (has_non_empty_default_host(api_set)) {
        return false;
      }

      std::string contract_name = tests::narrow_ascii(api_set.contract_name());
      return api_query.is_api_set_implemented(contract_name.c_str()) == FALSE;
    });

    require(unavailable_api_set != api_sets.end());

    for (const omni::api_set_host& host : unavailable_api_set->hosts()) {
      expect(host.value.empty());
    }
  };

  "find and lookup should accept canonical, versionless, and loader-style contract names"_test = [] {
    omni::api_sets api_sets{};
    auto versioned_api_set = api_sets.find_if([](const omni::api_set& api_set) {
      return find_contract_version_suffix(api_set.contract_name()) != std::wstring_view::npos;
    });

    require(versioned_api_set != api_sets.end());

    std::wstring_view contract_name = versioned_api_set->contract_name();
    auto version_suffix = find_contract_version_suffix(contract_name);
    std::wstring_view versionless_contract_name = contract_name.substr(0, version_suffix);

    auto exact_it = api_sets.find(omni::hash<omni::default_hash>(contract_name));
    auto versionless_it = api_sets.find(omni::hash<omni::default_hash>(versionless_contract_name));
    auto exact_lookup = api_sets.lookup(omni::hash<omni::default_hash>(contract_name));
    auto versionless_lookup = api_sets.lookup(omni::hash<omni::default_hash>(versionless_contract_name));

    require(exact_it != api_sets.end());
    require(versionless_it != api_sets.end());
    require(exact_lookup.has_value());
    require(versionless_lookup.has_value());
    expect(versionless_it->contract_name() == exact_it->contract_name());
    expect(versionless_lookup->contract_name() == exact_lookup->contract_name());
    expect(!api_sets.lookup(L"omni-contract-that-does-not-exist").has_value());
  };
}
