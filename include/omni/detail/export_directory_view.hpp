#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include "omni/address.hpp"
#include "omni/win/directories.hpp"
#include "omni/win/image.hpp"

namespace omni::detail {

  class export_directory_view {
   public:
    constexpr static std::size_t npos{static_cast<std::size_t>(-1)};

    export_directory_view() = default;

    explicit export_directory_view(omni::address module_base) noexcept
      : module_base_(module_base), export_dir_(win::get_export_directory(module_base)) {}

    [[nodiscard]] omni::address module_base() const noexcept {
      return module_base_;
    }

    [[nodiscard]] const win::export_directory* native_handle() const noexcept {
      return export_dir_;
    }

    [[nodiscard]] std::size_t functions_count() const noexcept {
      if (export_dir_ == nullptr) {
        return 0;
      }

      return export_dir_->num_functions;
    }

    [[nodiscard]] std::size_t names_count() const noexcept {
      if (export_dir_ == nullptr) {
        return 0;
      }

      return export_dir_->num_names;
    }

    [[nodiscard]] std::size_t function_index(std::size_t name_index) const noexcept {
      if (export_dir_ == nullptr || module_base_ == nullptr || name_index >= names_count()) {
        return npos;
      }

      auto* ordinal_table = export_dir_->ordinal_table(module_base_.value());

      return static_cast<std::size_t>(ordinal_table[name_index]);
    }

    [[nodiscard]] omni::address address(std::size_t function_index) const noexcept {
      if (export_dir_ == nullptr || module_base_ == nullptr || function_index >= functions_count()) {
        return {};
      }

      auto* rva_table = export_dir_->rva_table(module_base_.value());

      return module_base_.offset(rva_table[function_index]);
    }

    [[nodiscard]] std::uint32_t ordinal(std::size_t function_index) const noexcept {
      if (export_dir_ == nullptr || function_index >= functions_count()) {
        return 0;
      }

      return export_dir_->base + static_cast<std::uint32_t>(function_index);
    }

    [[nodiscard]] std::string_view name(std::size_t name_index) const noexcept {
      if (export_dir_ == nullptr || module_base_ == nullptr || name_index >= names_count()) {
        return {};
      }

      auto* names_table = export_dir_->names_table(module_base_.value());

      return std::string_view{module_base_.offset<const char*>(names_table[name_index])};
    }

    [[nodiscard]] bool is_forwarded(omni::address export_address) const noexcept {
      if (export_dir_ == nullptr) {
        return false;
      }

      const auto* image = module_base_.ptr<const win::image>();
      const auto export_data_dir = image->get_optional_header()->data_directories.export_directory;

      auto export_table_begin = module_base_.offset(export_data_dir.rva);
      auto export_table_end = export_table_begin.offset(export_data_dir.size);

      return export_address.is_in_range(export_table_begin, export_table_end);
    }

    [[nodiscard]] bool present() const noexcept {
      return module_base_ != nullptr && export_dir_ != nullptr;
    }

    [[nodiscard]] bool operator==(const export_directory_view&) const = default;

   private:
    omni::address module_base_;
    win::export_directory* export_dir_{nullptr};
  };

} // namespace omni::detail
