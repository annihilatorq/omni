#pragma once

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <string_view>

#include "omni/address.hpp"
#include "omni/concepts.hpp"
#include "omni/hash.hpp"
#include "omni/module_export.hpp"
#include "omni/win/directories.hpp"
#include "omni/win/image.hpp"

namespace omni {

  class module_exports {
    enum class iteration_kind : std::uint8_t {
      all,
      ordinal,
      named,
    };

   public:
    module_exports() = default;
    explicit module_exports(omni::address module_base) noexcept
      : module_base_(module_base), export_dir_(get_export_directory(module_base)) {}

    [[nodiscard]] std::size_t size() const noexcept {
      if (export_dir_ == nullptr) {
        return 0;
      }

      return export_dir_->num_functions;
    }

    [[nodiscard]] const win::export_directory* directory() const noexcept {
      return export_dir_;
    }

    [[nodiscard]] std::string_view name(std::size_t index) const noexcept {
      if (export_dir_ == nullptr || index >= size()) {
        return {};
      }

      const std::uint16_t* ordinal_table_ptr = export_dir_->ordinal_table(module_base_.value());
      const auto* rva_names_ptr = module_base_.offset<const std::uint32_t*>(export_dir_->rva_names);

      for (std::size_t name_index{}; name_index < export_dir_->num_names; ++name_index) {
        if (static_cast<std::size_t>(ordinal_table_ptr[name_index]) == index) {
          const auto* export_name_ptr = module_base_.offset<const char*>(rva_names_ptr[name_index]);
          return {export_name_ptr};
        }
      }

      return {};
    }

    [[nodiscard]] std::uint32_t ordinal(std::size_t index) const noexcept {
      if (export_dir_ == nullptr || index >= size()) {
        return 0;
      }

      return static_cast<std::uint32_t>(export_dir_->base + index);
    }

    [[nodiscard]] omni::address address(std::size_t index) const noexcept {
      if (export_dir_ == nullptr || index >= size()) {
        return {};
      }

      const auto* rva_table_ptr = export_dir_->rva_table(module_base_.value());
      const auto rva_function = rva_table_ptr[index];

      return module_base_.offset(rva_function);
    }

    [[nodiscard]] bool is_export_forwarded(omni::address export_address) const noexcept {
      if (export_dir_ == nullptr) {
        return false;
      }

      const auto* image = module_base_.ptr<const win::image>();
      const auto export_data_dir = image->get_optional_header()->data_directories.export_directory;

      const auto export_table_start = module_base_.offset(export_data_dir.rva);
      const auto export_table_end = export_table_start.offset(export_data_dir.size);

      return export_address >= export_table_start && export_address < export_table_end;
    }

    template <iteration_kind IterationKind>
    class export_iterator {
     public:
      using iterator_category = std::bidirectional_iterator_tag;
      using value_type = module_export;
      using difference_type = std::ptrdiff_t;
      using pointer = const value_type*;
      using reference = const value_type&;

      export_iterator() noexcept: module_exports_(nullptr), index_(0), current_export_() {}
      ~export_iterator() = default;
      export_iterator(const export_iterator&) = default;
      export_iterator(export_iterator&&) = default;
      export_iterator& operator=(export_iterator&&) = default;

      export_iterator(const module_exports* exports, std::size_t index) noexcept
        : module_exports_(exports), index_(index), current_export_() {
        update_current_export();
      }

      [[nodiscard]] reference operator*() const noexcept {
        return current_export_;
      }

      [[nodiscard]] pointer operator->() const noexcept {
        return &current_export_;
      }

      export_iterator& operator=(const export_iterator& other) noexcept {
        if (this != &other) {
          module_exports_ = other.module_exports_;
          index_ = other.index_;
          current_export_ = other.current_export_;
        }

        return *this;
      }

      export_iterator& operator++() noexcept {
        if (module_exports_ == nullptr) {
          return *this;
        }

        if (index_ < module_exports_->iteration_size<IterationKind>()) {
          ++index_;
        }

        update_current_export();
        return *this;
      }

      export_iterator operator++(int) noexcept {
        export_iterator temp = *this;
        ++(*this);
        return temp;
      }

      export_iterator& operator--() noexcept {
        if (module_exports_ == nullptr) {
          return *this;
        }

        if (index_ > 0) {
          --index_;
        }

        update_current_export();
        return *this;
      }

      export_iterator operator--(int) noexcept {
        export_iterator temp = *this;
        --(*this);
        return temp;
      }

      [[nodiscard]] bool operator==(const export_iterator& other) const noexcept {
        return index_ == other.index_ && module_exports_ == other.module_exports_;
      }

      [[nodiscard]] bool operator!=(const export_iterator& other) const noexcept {
        return !(*this == other);
      }

     private:
      void update_current_export() noexcept {
        if (module_exports_ == nullptr || index_ >= module_exports_->iteration_size<IterationKind>()) {
          current_export_ = value_type{};
          return;
        }

        // Fast path for named exports to avoid O(n) search
        if constexpr (IterationKind == iteration_kind::named) {
          const auto* ordinal_table_ptr = module_exports_->export_dir_->ordinal_table(module_exports_->module_base_.value());
          const auto* rva_names_ptr =
            module_exports_->module_base_.offset<const std::uint32_t*>(module_exports_->export_dir_->rva_names);

          const auto function_index = static_cast<std::size_t>(ordinal_table_ptr[index_]);
          const auto* export_name_ptr = module_exports_->module_base_.offset<const char*>(rva_names_ptr[index_]);
          const auto export_address = module_exports_->address(function_index);
          bool is_export_forwarded = module_exports_->is_export_forwarded(export_address);

          current_export_ = value_type{
            .name = std::string_view{export_name_ptr},
            .address = export_address,
            .ordinal = module_exports_->ordinal(function_index),
            .is_forwarded = is_export_forwarded,
            .module_base = module_exports_->module_base_,
          };

          if (is_export_forwarded) {
            current_export_.forwarder_string = forwarder_string::parse(export_address.ptr<const char>());
          }

          return;
        }

        const auto function_index = index_;
        const auto export_address = module_exports_->address(function_index);
        bool is_export_forwarded = module_exports_->is_export_forwarded(export_address);

        current_export_ = value_type{
          .name = get_export_name(function_index),
          .address = export_address,
          .ordinal = module_exports_->ordinal(function_index),
          .is_forwarded = is_export_forwarded,
          .module_base = module_exports_->module_base_,
        };

        if (is_export_forwarded) {
          current_export_.forwarder_string = forwarder_string::parse(export_address.ptr<const char>());
        }
      }

      [[nodiscard]] std::string_view get_export_name(std::size_t function_index) const noexcept {
        // Search for export name, which is O(n) only in case of
        // iterating over a range that includes named exports
        if constexpr (IterationKind == iteration_kind::all || IterationKind == iteration_kind::named) {
          return module_exports_->name(function_index);
        }
        return {};
      }

      const module_exports* module_exports_;
      std::size_t index_;
      mutable value_type current_export_;
    };

    template <iteration_kind IterationKind>
    class export_range {
     public:
      export_range() noexcept: module_exports_(nullptr) {}
      explicit export_range(const module_exports* exports) noexcept: module_exports_(exports) {}

      [[nodiscard]] export_iterator<IterationKind> begin() const noexcept {
        return {module_exports_, 0};
      }

      [[nodiscard]] export_iterator<IterationKind> end() const noexcept {
        if (module_exports_ == nullptr) {
          return {nullptr, 0};
        }

        return {module_exports_, module_exports_->iteration_size<IterationKind>()};
      }

     private:
      const module_exports* module_exports_;
    };

    using iterator = export_iterator<iteration_kind::all>;
    using ordinal_iterator = export_iterator<iteration_kind::ordinal>;
    using named_iterator = export_iterator<iteration_kind::named>;

    using all_range = export_range<iteration_kind::all>;
    using ordinal_range = export_range<iteration_kind::ordinal>;
    using named_range = export_range<iteration_kind::named>;

    static_assert(std::bidirectional_iterator<iterator>);
    static_assert(std::bidirectional_iterator<ordinal_iterator>);
    static_assert(std::bidirectional_iterator<named_iterator>);

    [[nodiscard]] iterator begin() const& noexcept {
      return {this, 0};
    }

    [[nodiscard]] iterator end() const& noexcept {
      return {this, iteration_size<iteration_kind::all>()};
    }

    [[nodiscard]] all_range all() const& noexcept {
      return all_range{this};
    }

    [[nodiscard]] ordinal_range ordinal() const& noexcept {
      return ordinal_range{this};
    }

    [[nodiscard]] named_range named() const& noexcept {
      return named_range{this};
    }

    [[nodiscard]] iterator find(concepts::hash auto export_name) const& noexcept {
      return find_by_name_hash(export_name);
    }

    [[nodiscard]] iterator find(default_hash export_name) const& noexcept {
      return find_by_name_hash(export_name);
    }

    [[nodiscard]] iterator find(std::uint32_t ordinal_value, omni::use_ordinal_t) const& noexcept {
      if (export_dir_ == nullptr || ordinal_value < export_dir_->base) {
        return end();
      }

      const auto function_index = static_cast<std::size_t>(ordinal_value - export_dir_->base);
      if (function_index >= size()) {
        return end();
      }

      return {this, function_index};
    }

    [[nodiscard]] iterator find_if(std::predicate<const typename iterator::value_type&> auto predicate) const& {
      if (export_dir_ == nullptr) {
        return end();
      }

      return std::ranges::find_if(*this, predicate);
    }

    // NOLINTBEGIN(modernize-use-nodiscard)

    iterator begin() const&& = delete;
    iterator end() const&& = delete;
    all_range all() const&& = delete;
    ordinal_range ordinal() const&& = delete;
    named_range named() const&& = delete;
    iterator find(concepts::hash auto export_name) const&& = delete;
    iterator find(default_hash export_name) const&& = delete;
    iterator find(std::uint32_t ordinal_value, omni::use_ordinal_t) const&& = delete;
    iterator find_if(std::predicate<const typename iterator::value_type&> auto predicate) const&& = delete;

    // NOLINTEND(modernize-use-nodiscard)

   private:
    template <iteration_kind IterationKind>
    [[nodiscard]] std::size_t iteration_size() const noexcept {
      if (export_dir_ == nullptr) {
        return 0;
      }

      if constexpr (IterationKind == iteration_kind::named) {
        return export_dir_->num_names;
      }

      return export_dir_->num_functions;
    }

    template <typename Hash>
    [[nodiscard]] iterator find_by_name_hash(Hash export_name) const noexcept {
      if (export_dir_ == nullptr) {
        return end();
      }

      const auto* ordinal_table_ptr = export_dir_->ordinal_table(module_base_.value());
      const auto* rva_names_ptr = module_base_.offset<const std::uint32_t*>(export_dir_->rva_names);

      for (std::size_t name_index{}; name_index < export_dir_->num_names; ++name_index) {
        const auto* export_name_ptr = module_base_.offset<const char*>(rva_names_ptr[name_index]);
        const auto export_name_view = std::string_view{export_name_ptr};

        if (!export_name_view.empty() && omni::hash<Hash>(export_name_view) == export_name) {
          return {this, static_cast<std::size_t>(ordinal_table_ptr[name_index])};
        }
      }

      return end();
    }

    static win::export_directory* get_export_directory(omni::address base_address) {
      if (!base_address) {
        return nullptr;
      }

      const auto* image = base_address.ptr<const win::image>();
      const auto export_data_dir = image->get_optional_header()->data_directories.export_directory;
      if (!export_data_dir.present()) {
        return nullptr;
      }

      return base_address.ptr<win::export_directory>(export_data_dir.rva);
    }

    omni::address module_base_;
    win::export_directory* export_dir_{nullptr};
  };

  static_assert(std::ranges::viewable_range<module_exports>);
  static_assert(std::ranges::viewable_range<module_exports::all_range>);
  static_assert(std::ranges::viewable_range<module_exports::ordinal_range>);
  static_assert(std::ranges::viewable_range<module_exports::named_range>);

} // namespace omni
