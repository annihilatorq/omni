#pragma once

#include <cstddef>
#include <iterator>
#include <ranges>

#include "omni/address.hpp"
#include "omni/concepts/export_range.hpp"
#include "omni/detail/export_directory_view.hpp"
#include "omni/module_export.hpp"

namespace omni {

  class ordinal_exports {
   public:
    ordinal_exports() = default;
    explicit ordinal_exports(omni::address module_base) noexcept: export_dir_view_(module_base) {}

    [[nodiscard]] std::size_t size() const noexcept {
      return export_dir_view_.functions_count();
    }

    [[nodiscard]] std::uint32_t ordinal(std::size_t index) const noexcept {
      return export_dir_view_.ordinal(index);
    }

    [[nodiscard]] omni::address address(std::size_t index) const noexcept {
      return export_dir_view_.address(index);
    }

    [[nodiscard]] bool is_forwarded(omni::address export_address) const noexcept {
      return export_dir_view_.is_forwarded(export_address);
    }

    [[nodiscard]] const win::export_directory* directory() const noexcept {
      return export_dir_view_.native_handle();
    }

    class iterator {
     public:
      using iterator_category = std::bidirectional_iterator_tag;
      using value_type = ordinal_export;
      using difference_type = std::ptrdiff_t;
      using pointer = const value_type*;
      using reference = const value_type&;

      iterator() noexcept = default;
      ~iterator() = default;
      iterator(const iterator&) = default;
      iterator(iterator&&) = default;
      iterator& operator=(iterator&&) = default;

      iterator(detail::export_directory_view export_dir_view, std::size_t index) noexcept
        : export_dir_view_(export_dir_view), index_(index) {
        update_current_export();
      }

      [[nodiscard]] reference operator*() const noexcept {
        return current_export_;
      }

      [[nodiscard]] pointer operator->() const noexcept {
        return &current_export_;
      }

      iterator& operator=(const iterator& other) noexcept {
        if (this != &other) {
          export_dir_view_ = other.export_dir_view_;
          index_ = other.index_;
          current_export_ = other.current_export_;
        }

        return *this;
      }

      iterator& operator++() noexcept {
        if (export_dir_view_.present() && index_ < export_dir_view_.functions_count()) {
          ++index_;
        }

        update_current_export();
        return *this;
      }

      iterator operator++(int) noexcept {
        iterator temp = *this;
        ++(*this);
        return temp;
      }

      iterator& operator--() noexcept {
        if (export_dir_view_.present() && index_ > 0) {
          --index_;
        }

        update_current_export();
        return *this;
      }

      iterator operator--(int) noexcept {
        iterator temp = *this;
        --(*this);
        return temp;
      }

      [[nodiscard]] bool operator==(const iterator& other) const noexcept {
        return index_ == other.index_ && export_dir_view_ == other.export_dir_view_;
      }

      [[nodiscard]] bool operator!=(const iterator& other) const noexcept {
        return !(*this == other);
      }

     private:
      void update_current_export() noexcept {
        if (!export_dir_view_.present() || index_ >= export_dir_view_.functions_count()) {
          current_export_ = value_type{};
          return;
        }

        const auto export_address = export_dir_view_.address(index_);

        current_export_ = value_type{
          .ordinal = export_dir_view_.ordinal(index_),
          .address = export_address,
          .module_base = export_dir_view_.module_base(),
        };

        if (export_dir_view_.is_forwarded(export_address)) {
          current_export_.forwarder_string = forwarder_string::parse(export_address.ptr<const char>());
        }
      }

      detail::export_directory_view export_dir_view_;
      std::size_t index_{0};
      mutable value_type current_export_{};
    };

    static_assert(std::bidirectional_iterator<iterator>);

    [[nodiscard]] iterator begin() const noexcept {
      return {export_dir_view_, 0};
    }

    [[nodiscard]] iterator end() const noexcept {
      return {export_dir_view_, size()};
    }

    [[nodiscard]] iterator find(std::uint32_t ordinal) const noexcept {
      if (directory() == nullptr || ordinal < directory()->base) {
        return end();
      }

      const auto function_index = static_cast<std::size_t>(ordinal - directory()->base);
      if (function_index >= size()) {
        return end();
      }

      return {export_dir_view_, function_index};
    }

    [[nodiscard]] iterator find_if(std::predicate<const typename iterator::value_type&> auto predicate) const {
      if (directory() == nullptr) {
        return end();
      }

      return std::ranges::find_if(*this, predicate);
    }

   private:
    [[nodiscard]] omni::address module_base() const noexcept {
      return export_dir_view_.module_base();
    }

    detail::export_directory_view export_dir_view_;
  };

  static_assert(omni::concepts::export_range<ordinal_exports, std::uint32_t, ordinal_export>);
  static_assert(std::ranges::viewable_range<ordinal_exports>);

} // namespace omni
