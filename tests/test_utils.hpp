#pragma once

#include <Windows.h>

#include <array>
#include <filesystem>
#include <span>
#include <string>
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
using ut::operator""_test;

#ifdef OMNI_HAS_EXCEPTIONS
using ut::throws;
#endif

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

  using is_api_set_implemented_fn = BOOL(APIENTRY*)(PCSTR);
  using get_api_set_module_base_name_fn = HRESULT(APIENTRY*)(PCSTR, UINT32, PWSTR, UINT32*);

  struct api_set_query_api {
    api_set_query_api() noexcept: apiquery2{L"api-ms-win-core-apiquery-l2-1-0.dll"} {
      HMODULE query_module = apiquery2.handle;
      if (query_module == nullptr) {
        query_module = ::GetModuleHandleW(L"kernelbase.dll");
      }

      if (query_module != nullptr) {
        is_api_set_implemented =
          reinterpret_cast<is_api_set_implemented_fn>(::GetProcAddress(query_module, "IsApiSetImplemented"));
        get_api_set_module_base_name =
          reinterpret_cast<get_api_set_module_base_name_fn>(::GetProcAddress(query_module, "GetApiSetModuleBaseName"));
      }
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return is_api_set_implemented != nullptr;
    }

    loaded_library apiquery2;
    is_api_set_implemented_fn is_api_set_implemented{};
    get_api_set_module_base_name_fn get_api_set_module_base_name{};
  };

  struct api_set_module_base_name_result {
    HRESULT hr{};
    std::wstring module_base_name;
  };

  [[nodiscard]] inline std::string narrow_ascii(std::wstring_view value) {
    std::string result{};
    result.reserve(value.size());

    for (wchar_t ch : value) {
      result.push_back(static_cast<char>(ch));
    }

    return result;
  }

  [[nodiscard]] inline api_set_module_base_name_result query_api_set_module_base_name(const api_set_query_api& api_query,
    std::string_view contract_name) {
    if (api_query.get_api_set_module_base_name != nullptr) {
      std::array<wchar_t, MAX_PATH> module_base_name_buffer{};
      std::string contract_name_storage{contract_name};
      UINT32 actual_name_length{};
      HRESULT hr = api_query.get_api_set_module_base_name(contract_name_storage.c_str(),
        static_cast<UINT32>(module_base_name_buffer.size()),
        module_base_name_buffer.data(),
        &actual_name_length);

      if (SUCCEEDED(hr)) {
        std::wstring module_base_name{module_base_name_buffer.data(),
          actual_name_length == 0U ? 0U : static_cast<std::size_t>(actual_name_length - 1)};

        return {
          .hr = hr,
          .module_base_name = std::move(module_base_name),
        };
      }

      if (hr != E_NOTIMPL) {
        return {.hr = hr};
      }
    }

    std::wstring contract_name_storage{contract_name.begin(), contract_name.end()};
    if (!contract_name_storage.ends_with(L".dll")) {
      contract_name_storage += L".dll";
    }

    loaded_library api_set_module{contract_name_storage.c_str()};
    if (!api_set_module) {
      return {.hr = E_FAIL};
    }

    auto module_path = get_module_path(api_set_module.handle);
    return {
      .hr = S_OK,
      .module_base_name = module_path.filename().wstring(),
    };
  }

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
