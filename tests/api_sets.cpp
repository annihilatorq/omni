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

  [[nodiscard]] std::size_t find_contract_version_suffix(std::wstring_view contract_name) {
    auto cursor = contract_name.size();

    const auto consume_decimal = [&contract_name](std::size_t& index) {
      auto end = index;
      while (index > 0 && contract_name[index - 1] >= L'0' && contract_name[index - 1] <= L'9') {
        --index;
      }

      return index != end;
    };

    if (!consume_decimal(cursor) || cursor == 0 || contract_name[cursor - 1] != L'-') {
      return std::wstring_view::npos;
    }
    --cursor;

    if (!consume_decimal(cursor) || cursor == 0 || contract_name[cursor - 1] != L'-') {
      return std::wstring_view::npos;
    }
    --cursor;

    if (!consume_decimal(cursor) || cursor < 2 || contract_name[cursor - 1] != L'l' || contract_name[cursor - 2] != L'-') {
      return std::wstring_view::npos;
    }

    return cursor - 2;
  }

  [[nodiscard]] std::string_view resolution_reason(tests::api_set_module_base_name_resolution resolution) {
    switch (resolution) {
    case tests::api_set_module_base_name_resolution::direct_query:
      return "direct query";
    case tests::api_set_module_base_name_resolution::fallback_missing_api:
      return "GetApiSetModuleBaseName is unavailable";
    case tests::api_set_module_base_name_resolution::fallback_e_notimpl:
      return "GetApiSetModuleBaseName returned E_NOTIMPL";
    }

    return "unknown resolution path";
  }

} // namespace

int main() {
  ut::suite api_sets_suite = [] {
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
      std::size_t checked_contracts{};
      std::size_t mismatched_contracts{};
      std::size_t fallback_missing_api_count{};
      std::size_t fallback_e_notimpl_count{};
      bool logged_missing_api_fallback{};
      bool logged_e_notimpl_fallback{};

      require(static_cast<bool>(api_query));

      for (const omni::api_set& api_set : omni::api_sets{}) {
        std::string contract_name = tests::narrow_ascii(api_set.contract_name());
        if (api_query.is_api_set_implemented(contract_name.c_str()) == FALSE || !has_non_empty_default_host(api_set)) {
          continue;
        }

        auto module_base_name = tests::query_api_set_module_base_name(api_query, contract_name);
        switch (module_base_name.resolution) {
        case tests::api_set_module_base_name_resolution::direct_query:
          break;
        case tests::api_set_module_base_name_resolution::fallback_missing_api:
          ++fallback_missing_api_count;
          if (!logged_missing_api_fallback) {
            std::cerr << "[omni::api_sets] falling back to LoadLibrary-based API-set resolution because "
                      << resolution_reason(module_base_name.resolution) << "; first contract=\"" << contract_name << "\"\n";
            logged_missing_api_fallback = true;
          }
          break;
        case tests::api_set_module_base_name_resolution::fallback_e_notimpl:
          ++fallback_e_notimpl_count;
          if (!logged_e_notimpl_fallback) {
            std::cerr << "[omni::api_sets] falling back to LoadLibrary-based API-set resolution because "
                      << resolution_reason(module_base_name.resolution) << "; first contract=\"" << contract_name << "\"\n";
            logged_e_notimpl_fallback = true;
          }
          break;
        }

        if (FAILED(module_base_name.hr)) {
          continue;
        }

        auto default_host = api_set.default_host();
        auto resolved_host = api_set.resolve_host();
        bool host_match = default_host.has_value() && resolved_host.has_value() &&
                          equal_module_name(default_host->value, module_base_name.module_base_name) &&
                          equal_module_name(resolved_host->value, module_base_name.module_base_name);

        ++checked_contracts;
        if (!host_match) {
          ++mismatched_contracts;
        }
      }

      if (fallback_missing_api_count != 0U || fallback_e_notimpl_count != 0U) {
        std::cerr << "[omni::api_sets] fallback summary: missing_api=" << fallback_missing_api_count
                  << ", e_notimpl=" << fallback_e_notimpl_count << '\n';
      }

      expect(checked_contracts > 0U);
      expect(mismatched_contracts == 0U);
    };

    "contracts without an implementation are reported unavailable by Windows"_test = [] {
      tests::api_set_query_api api_query{};
      omni::api_sets api_sets{};
      auto unavailable_api_set = api_sets.find_if([&api_query](const omni::api_set& api_set) {
        std::string contract_name = tests::narrow_ascii(api_set.contract_name());
        return api_query.is_api_set_implemented(contract_name.c_str()) == FALSE && !has_non_empty_default_host(api_set);
      });

      require(static_cast<bool>(api_query));
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
  };
}
