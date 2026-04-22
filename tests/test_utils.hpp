#pragma once

#include <Windows.h>

#include <filesystem>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "omni/address.hpp"
#include "omni/module.hpp"
#include "omni/modules.hpp"

#include <boost/ut.hpp>

namespace ut = boost::ut;
using ut::expect;
using ut::fatal;
using ut::throws;
using ut::operator""_test;

namespace omni::tests {

  struct manual_export_info {
    std::size_t function_index{};
    std::string_view name;
    omni::address address;
    std::uint32_t ordinal{};
    bool is_forwarded{};
  };

  [[nodiscard]] inline std::filesystem::path get_module_path(HMODULE module_handle) {
    std::array<wchar_t, 32768> buffer{};
    auto length = ::GetModuleFileNameW(module_handle, buffer.data(), static_cast<DWORD>(buffer.size()));
    return {std::wstring_view{buffer.data(), length}};
  }

  [[nodiscard]] inline omni::module get_loaded_module(HMODULE module_handle) {
    return omni::get_module(omni::address{module_handle});
  }

  struct loaded_library {
    explicit loaded_library(const wchar_t* module_name) noexcept
      : handle{::LoadLibraryExW(module_name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)} {
      if (handle == nullptr) {
        handle = ::LoadLibraryW(module_name);
      }
    }

    loaded_library(const loaded_library&) = delete;
    loaded_library& operator=(const loaded_library&) = delete;

    loaded_library(loaded_library&& other) noexcept: handle{std::exchange(other.handle, nullptr)} {}
    loaded_library& operator=(loaded_library&& other) noexcept {
      if (this != &other) {
        if (handle != nullptr) {
          ::FreeLibrary(handle);
        }
        handle = std::exchange(other.handle, nullptr);
      }
      return *this;
    }

    ~loaded_library() {
      if (handle != nullptr) {
        ::FreeLibrary(handle);
      }
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return handle != nullptr;
    }

    HMODULE handle{};
  };

  [[nodiscard]] inline const IMAGE_NT_HEADERS* get_nt_headers(HMODULE module_handle) {
    auto* dos_header = reinterpret_cast<const IMAGE_DOS_HEADER*>(module_handle);
    return reinterpret_cast<const IMAGE_NT_HEADERS*>(reinterpret_cast<const std::byte*>(module_handle) + dos_header->e_lfanew);
  }

  [[nodiscard]] inline IMAGE_DATA_DIRECTORY get_export_data_directory(HMODULE module_handle) {
    return get_nt_headers(module_handle)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
  }

  [[nodiscard]] inline const IMAGE_EXPORT_DIRECTORY* get_export_directory(HMODULE module_handle) {
    auto export_data_directory = get_export_data_directory(module_handle);
    if (export_data_directory.VirtualAddress == 0 || export_data_directory.Size == 0) {
      return nullptr;
    }

    return reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
      reinterpret_cast<const std::byte*>(module_handle) + export_data_directory.VirtualAddress);
  }

  [[nodiscard]] inline std::vector<manual_export_info> get_export_table_entries(HMODULE module_handle) {
    const IMAGE_EXPORT_DIRECTORY* export_directory = get_export_directory(module_handle);
    if (export_directory == nullptr) {
      return {};
    }

    auto export_data_directory = get_export_data_directory(module_handle);
    auto* module_base = reinterpret_cast<const std::byte*>(module_handle);
    auto* function_table = reinterpret_cast<const std::uint32_t*>(module_base + export_directory->AddressOfFunctions);
    auto* name_table = reinterpret_cast<const std::uint32_t*>(module_base + export_directory->AddressOfNames);
    auto* ordinal_table = reinterpret_cast<const std::uint16_t*>(module_base + export_directory->AddressOfNameOrdinals);

    std::vector<std::string_view> export_names(export_directory->NumberOfFunctions);
    for (std::size_t name_index{}; name_index < export_directory->NumberOfNames; ++name_index) {
      auto function_index = static_cast<std::size_t>(ordinal_table[name_index]);
      export_names[function_index] = reinterpret_cast<const char*>(module_base + name_table[name_index]);
    }

    omni::address export_table_begin = omni::address{module_handle}.offset(export_data_directory.VirtualAddress);
    omni::address export_table_end = export_table_begin.offset(export_data_directory.Size);

    std::vector<manual_export_info> export_entries{};
    export_entries.reserve(export_directory->NumberOfFunctions);

    for (std::size_t function_index{}; function_index < export_directory->NumberOfFunctions; ++function_index) {
      omni::address export_address = omni::address{module_handle}.offset(function_table[function_index]);

      export_entries.push_back({
        .function_index = function_index,
        .name = export_names[function_index],
        .address = export_address,
        .ordinal = export_directory->Base + static_cast<std::uint32_t>(function_index),
        .is_forwarded = export_address >= export_table_begin && export_address < export_table_end,
      });
    }

    return export_entries;
  }

  [[nodiscard]] inline std::vector<manual_export_info> get_named_export_table_entries(HMODULE module_handle) {
    const IMAGE_EXPORT_DIRECTORY* export_directory = get_export_directory(module_handle);
    if (export_directory == nullptr) {
      return {};
    }

    auto export_data_directory = get_export_data_directory(module_handle);
    auto* module_base = reinterpret_cast<const std::byte*>(module_handle);
    auto* function_table = reinterpret_cast<const std::uint32_t*>(module_base + export_directory->AddressOfFunctions);
    auto* name_table = reinterpret_cast<const std::uint32_t*>(module_base + export_directory->AddressOfNames);
    auto* ordinal_table = reinterpret_cast<const std::uint16_t*>(module_base + export_directory->AddressOfNameOrdinals);

    omni::address export_table_begin = omni::address{module_handle}.offset(export_data_directory.VirtualAddress);
    omni::address export_table_end = export_table_begin.offset(export_data_directory.Size);

    std::vector<manual_export_info> named_export_entries{};
    named_export_entries.reserve(export_directory->NumberOfNames);

    for (std::size_t name_index{}; name_index < export_directory->NumberOfNames; ++name_index) {
      auto function_index = static_cast<std::size_t>(ordinal_table[name_index]);
      omni::address export_address = omni::address{module_handle}.offset(function_table[function_index]);

      named_export_entries.push_back({
        .function_index = function_index,
        .name = reinterpret_cast<const char*>(module_base + name_table[name_index]),
        .address = export_address,
        .ordinal = export_directory->Base + static_cast<std::uint32_t>(function_index),
        .is_forwarded = export_address >= export_table_begin && export_address < export_table_end,
      });
    }

    return named_export_entries;
  }

  inline const manual_export_info* find_export_by_name(std::span<manual_export_info> export_entries,
    std::string_view export_name) {
    for (const manual_export_info& export_entry : export_entries) {
      if (export_entry.name == export_name) {
        return &export_entry;
      }
    }

    return nullptr;
  }

  inline const manual_export_info* find_first_forwarded_export(std::span<manual_export_info> export_entries) {
    for (const manual_export_info& export_entry : export_entries) {
      if (export_entry.is_forwarded) {
        return &export_entry;
      }
    }

    return nullptr;
  }

} // namespace omni::tests
