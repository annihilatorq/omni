#pragma once

#include <cstdint>
#include <ranges>
#include <span>
#include <string_view>

#include "omni/api_set.hpp"
#include "omni/concepts/concepts.hpp"
#include "omni/hash.hpp"
#include "omni/win/api_set_map.hpp"
#include "omni/win/peb.hpp"

namespace omni {

  namespace detail {

    [[nodiscard]] constexpr bool is_decimal(std::wstring_view value) noexcept {
      return !value.empty() && std::ranges::all_of(value, [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; });
    }

    [[nodiscard]] constexpr std::size_t find_api_set_version_suffix(std::wstring_view contract_name) noexcept {
      const std::size_t patch_dash_pos = contract_name.rfind(L'-');
      if (patch_dash_pos == std::wstring_view::npos || patch_dash_pos == 0 ||
          !is_decimal(contract_name.substr(patch_dash_pos + 1))) {
        return std::wstring_view::npos;
      }

      const std::size_t minor_dash_pos = contract_name.rfind(L'-', patch_dash_pos - 1);
      if (minor_dash_pos == std::wstring_view::npos || minor_dash_pos == 0 ||
          !is_decimal(contract_name.substr(minor_dash_pos + 1, patch_dash_pos - minor_dash_pos - 1))) {
        return std::wstring_view::npos;
      }

      const std::size_t level_dash_pos = contract_name.rfind(L'-', minor_dash_pos - 1);
      if (level_dash_pos == std::wstring_view::npos) {
        return std::wstring_view::npos;
      }

      const auto level = contract_name.substr(level_dash_pos + 1, minor_dash_pos - level_dash_pos - 1);
      if (level.size() < 2 || level.front() != L'l' || !is_decimal(level.substr(1))) {
        return std::wstring_view::npos;
      }

      return level_dash_pos;
    }

    [[nodiscard]] constexpr std::wstring_view remove_api_set_version(std::wstring_view contract_name) noexcept {
      auto version_pos = find_api_set_version_suffix(contract_name);
      if (version_pos == std::wstring_view::npos) {
        return {};
      }

      return contract_name.substr(0, version_pos);
    }

    [[nodiscard]] constexpr std::wstring_view remove_dll_suffix(std::wstring_view contract_name) noexcept {
      constexpr std::size_t dll_suffix_length = 4; // ".dll"

      if (contract_name.size() < dll_suffix_length) {
        return contract_name;
      }

      auto suffix = contract_name.substr(contract_name.size() - dll_suffix_length);
      if (omni::hash<omni::default_hash>(suffix) != omni::default_hash{L".dll"}) {
        return contract_name;
      }

      return contract_name.substr(0, contract_name.size() - dll_suffix_length);
    }

  } // namespace detail

  class api_sets {
   public:
    api_sets(): api_set_map_(win::PEB::ptr()->api_set_map) {}

    class iterator {
     public:
      using iterator_category = std::bidirectional_iterator_tag;
      using value_type = api_set;
      using difference_type = std::ptrdiff_t;
      using pointer = const value_type*;
      using reference = const value_type&;

      iterator() noexcept: api_set_map_(nullptr), index_(0) {}

      iterator(win::api_set_namespace* api_set_map, std::uint32_t index): api_set_map_(api_set_map), index_(index) {
        update_value();
      }

      [[nodiscard]] reference operator*() const noexcept {
        return current_;
      }

      [[nodiscard]] pointer operator->() const noexcept {
        return &current_;
      }

      iterator& operator++() noexcept {
        if (api_set_map_ == nullptr) {
          return *this;
        }

        if (index_ < api_set_map_->entries_count) {
          ++index_;
          update_value();
        }
        return *this;
      }

      iterator operator++(int) noexcept {
        iterator tmp = *this;
        ++(*this);
        return tmp;
      }

      iterator& operator--() noexcept {
        if (api_set_map_ == nullptr) {
          return *this;
        }

        if (index_ > 0) {
          --index_;
          update_value();
        }
        return *this;
      }

      iterator operator--(int) noexcept {
        iterator tmp = *this;
        --(*this);
        return tmp;
      }

      bool operator==(const iterator& other) const noexcept {
        return api_set_map_ == other.api_set_map_ && index_ == other.index_;
      }

      bool operator!=(const iterator& other) const noexcept {
        return !(*this == other);
      }

     private:
      void update_value() {
        if (api_set_map_ == nullptr || index_ >= api_set_map_->entries_count) {
          current_ = value_type{};
          return;
        }

        omni::address api_set_map_address{api_set_map_};

        const win::api_set_namespace_entry* namespace_entry = api_set_map_->get_namespace_entry(index_);
        std::span value_entries = namespace_entry->value_entries(api_set_map_address);

        bool is_entry_sealed = namespace_entry->is_sealed();
        std::wstring_view contract_name = namespace_entry->contract_name(api_set_map_address);

        current_ = value_type{contract_name, is_entry_sealed, value_entries, omni::address{api_set_map_}};
      }

      win::api_set_namespace* api_set_map_;
      std::uint32_t index_;
      mutable value_type current_;
    };

    static_assert(std::bidirectional_iterator<iterator>);

    [[nodiscard]] iterator begin() const noexcept {
      return {api_set_map_, 0};
    }

    [[nodiscard]] iterator end() const noexcept {
      return {api_set_map_, static_cast<std::uint32_t>(size())};
    }

    [[nodiscard]] std::size_t size() const noexcept {
      if (api_set_map_ == nullptr) {
        return 0;
      }

      return api_set_map_->entries_count;
    }

    [[nodiscard]] iterator find(concepts::hash auto contract_name_hash) const noexcept {
      using hash_type = decltype(contract_name_hash);

      for (auto it = begin(); it != end(); ++it) {
        auto canonical_contract_name = detail::remove_dll_suffix(it->contract_name());
        if (contract_name_hash == omni::hash<hash_type>(canonical_contract_name)) {
          return it;
        }

        auto versionless_contract_name = detail::remove_api_set_version(canonical_contract_name);
        if (versionless_contract_name.empty()) {
          continue;
        }

        if (contract_name_hash == omni::hash<hash_type>(versionless_contract_name)) {
          return it;
        }
      }

      return end();
    }

    [[nodiscard]] iterator find(default_hash contract_name) const noexcept {
      return find<default_hash>(contract_name);
    }

    [[nodiscard]] iterator find_if(std::predicate<iterator::value_type> auto pred) const {
      for (auto it = begin(); it != end(); ++it) {
        if (pred(*it)) {
          return it;
        }
      }
      return end();
    }

   private:
    win::api_set_namespace* api_set_map_{};
  };

  static_assert(std::ranges::viewable_range<api_sets>);

  inline omni::api_set get_api_set(concepts::hash auto contract_name) noexcept {
    omni::api_sets api_sets;
    auto it = api_sets.find(contract_name);
    if (it == api_sets.end()) {
      return {};
    }
    return *it;
  }

  inline omni::api_set get_api_set(default_hash contract_name) noexcept {
    return get_api_set<default_hash>(contract_name);
  }

} // namespace omni
