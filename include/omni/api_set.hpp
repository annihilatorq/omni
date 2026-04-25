#pragma once

#include <algorithm>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>

#include "omni/address.hpp"
#include "omni/win/api_set_map.hpp"

namespace omni {

  struct api_set_host {
    std::wstring_view value;
    std::wstring_view alias;

    [[nodiscard]] bool present() const noexcept {
      return !value.empty() || !alias.empty();
    }

    [[nodiscard]] bool is_default() const noexcept {
      return alias.empty();
    }
  };

  class api_set_hosts {
   public:
    api_set_hosts() noexcept = default;

    api_set_hosts(std::span<const win::api_set_value_entry> entries, omni::address api_set_map_address) noexcept
      : entries_(entries), api_set_map_address_(api_set_map_address) {}

    class iterator {
     public:
      using iterator_category = std::forward_iterator_tag;
      using value_type = api_set_host;
      using difference_type = std::ptrdiff_t;
      using pointer = const value_type*;
      using reference = const value_type&;

      iterator() noexcept: index_(0), api_set_map_address_(0) {}

      iterator(std::span<const win::api_set_value_entry> entries, omni::address api_set_map_address, std::size_t index = 0)
        : entries_(entries), index_(index), api_set_map_address_(api_set_map_address) {
        update_value();
      }

      [[nodiscard]] reference operator*() const noexcept {
        return current_;
      }

      [[nodiscard]] pointer operator->() const noexcept {
        return &current_;
      }

      iterator& operator++() noexcept {
        if (index_ < entries_.size()) {
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

      bool operator==(const iterator& other) const noexcept {
        return entries_.data() == other.entries_.data() && entries_.size() == other.entries_.size() && index_ == other.index_ &&
               api_set_map_address_ == other.api_set_map_address_;
      }

      bool operator!=(const iterator& other) const noexcept {
        return !(*this == other);
      }

     private:
      void update_value() noexcept {
        if (entries_.empty() || index_ >= entries_.size()) {
          current_ = value_type{};
          return;
        }

        const win::api_set_value_entry& entry = entries_[index_];
        current_ = value_type{
          .value = entry.value(api_set_map_address_),
          .alias = entry.alias(api_set_map_address_),
        };
      }

      std::span<const win::api_set_value_entry> entries_;
      std::size_t index_;
      omni::address api_set_map_address_;
      mutable value_type current_;
    };

    static_assert(std::forward_iterator<iterator>);

    [[nodiscard]] iterator begin() const {
      return {entries_, api_set_map_address_, 0};
    }

    [[nodiscard]] iterator end() const {
      return {entries_, api_set_map_address_, size()};
    }

    [[nodiscard]] std::size_t size() const noexcept {
      return entries_.size();
    }

   private:
    std::span<const win::api_set_value_entry> entries_;
    omni::address api_set_map_address_;
  };

  static_assert(std::ranges::viewable_range<api_set_hosts>);

  class api_set {
   public:
    api_set() = default;

    api_set(std::wstring_view contract_name, bool sealed, std::span<const win::api_set_value_entry> entries,
      omni::address base) noexcept
      : contract_name_(contract_name), is_sealed_(sealed), value_entries_(entries), base_(base) {}

    // Contract, for example: "api-ms-win-core-com-l1-1-0"
    [[nodiscard]] std::wstring_view contract_name() const noexcept {
      return contract_name_;
    }

    [[nodiscard]] bool is_sealed() const noexcept {
      return is_sealed_;
    }

    [[nodiscard]] api_set_hosts hosts() const noexcept {
      return {value_entries_, base_};
    }

    [[nodiscard]] std::optional<api_set_host> default_host() const noexcept {
      return find_host_entry({});
    }

    [[nodiscard]] std::optional<api_set_host> resolve_host(std::wstring_view alias = {}) const noexcept {
      auto exact_host = find_host_entry(alias);
      if (exact_host.has_value()) {
        return exact_host;
      }

      if (alias.empty()) {
        return std::nullopt;
      }

      return default_host();
    }

    [[nodiscard]] bool present() const noexcept {
      return !contract_name_.empty() && base_ != nullptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return present();
    }

   private:
    [[nodiscard]] std::optional<api_set_host> find_host_entry(std::wstring_view alias) const noexcept {
      auto all_hosts = hosts();
      auto host = std::ranges::find_if(all_hosts, [alias](const api_set_host& candidate) { return candidate.alias == alias; });
      if (host == all_hosts.end()) {
        return std::nullopt;
      }

      return *host;
    }

    std::wstring_view contract_name_;
    bool is_sealed_{};

    std::span<const win::api_set_value_entry> value_entries_;
    omni::address base_;
  };

} // namespace omni
