#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include "omni/address.hpp"

namespace omni::win {

  struct api_set_namespace;

  struct api_set_hash_entry {
    std::uint32_t hash;
    std::uint32_t index;
  };

  struct api_set_value_entry {
    std::uint32_t flags;
    std::uint32_t name_offset;
    std::uint32_t name_length;
    std::uint32_t value_offset;
    std::uint32_t value_length;

    [[nodiscard]] std::wstring_view value(omni::address api_set_namespace) const noexcept {
      if (value_length == 0) {
        return {};
      }

      auto* value_string_ptr = api_set_namespace.offset<wchar_t*>(value_offset);
      return {value_string_ptr, value_length / sizeof(wchar_t)};
    }

    [[nodiscard]] std::wstring_view alias(omni::address api_set_namespace) const noexcept {
      if (name_length == 0) {
        return {};
      }

      auto* name_string_ptr = api_set_namespace.offset<wchar_t*>(name_offset);
      return {name_string_ptr, name_length / sizeof(wchar_t)};
    }
  };

  struct api_set_namespace_entry {
    std::uint32_t flags;
    std::uint32_t name_offset;
    std::uint32_t name_length;
    std::uint32_t hashed_length;
    std::uint32_t value_offset;
    std::uint32_t value_count;

    [[nodiscard]] std::span<const api_set_value_entry> value_entries(omni::address api_set_namespace) const noexcept {
      const auto* first_entry = api_set_namespace.offset<const api_set_value_entry*>(value_offset);
      return {first_entry, value_count};
    }

    [[nodiscard]] bool is_sealed() const noexcept {
      return (flags & 1U) != 0;
    }

    [[nodiscard]] std::wstring_view contract_name(omni::address api_set_namespace) const noexcept {
      auto* name_string_ptr = api_set_namespace.offset<wchar_t*>(name_offset);
      return {name_string_ptr, name_length / sizeof(wchar_t)};
    }

    [[nodiscard]] api_set_value_entry* get_value_entry(omni::address api_set_namespace) const noexcept {
      return api_set_namespace.offset<api_set_value_entry*>(value_offset);
    }
  };

  struct api_set_namespace {
    std::uint32_t version;
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t entries_count;
    std::uint32_t entry_offset;
    std::uint32_t hash_offset;
    std::uint32_t hash_factor;

    [[nodiscard]] std::span<const api_set_namespace_entry> namespace_entries() const noexcept {
      const auto* first_entry =
        reinterpret_cast<const api_set_namespace_entry*>(reinterpret_cast<const std::byte*>(this) + entry_offset);

      return {first_entry, entries_count};
    }

    [[nodiscard]] api_set_namespace_entry* get_namespace_entry(std::size_t index) const noexcept {
      return omni::address{this}.offset<api_set_namespace_entry*>(entry_offset) + index;
    }
  };

} // namespace omni::win
