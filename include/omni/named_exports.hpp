#pragma once

#include <cstddef>
#include <iterator>
#include <ranges>
#include <string_view>

#include "omni/address.hpp"
#include "omni/concepts/concepts.hpp"
#include "omni/concepts/export_range.hpp"
#include "omni/detail/export_directory_view.hpp"
#include "omni/hash.hpp"
#include "omni/module_export.hpp"

namespace omni {

  class named_exports {
   public:
    named_exports() = default;
    explicit named_exports(omni::address module_base) noexcept: export_dir_view_(module_base) {}

    [[nodiscard]] std::size_t size() const noexcept {
      return export_dir_view_.names_count();
    }

    [[nodiscard]] const win::export_directory* directory() const noexcept {
      return export_dir_view_.native_handle();
    }

    [[nodiscard]] std::string_view name(std::size_t index) const noexcept {
      return export_dir_view_.name(index);
    }

    [[nodiscard]] omni::address address(std::size_t index) const noexcept {
      const auto function_index = export_dir_view_.function_index(index);
      if (function_index == detail::export_directory_view::npos) {
        return {};
      }

      return export_dir_view_.address(function_index);
    }

    [[nodiscard]] bool is_forwarded(omni::address export_address) const noexcept {
      return export_dir_view_.is_forwarded(export_address);
    }

    class iterator {
     public:
      using iterator_category = std::bidirectional_iterator_tag;
      using value_type = named_export;
      using difference_type = std::ptrdiff_t;
      using pointer = const value_type*;
      using reference = const value_type&;

      iterator() noexcept = default;
      ~iterator() = default;
      iterator(const iterator&) = default;
      iterator(iterator&&) = default;
      iterator& operator=(iterator&&) = default;

      iterator(detail::export_directory_view export_dir_view, std::size_t index) noexcept
        : export_dir_view_(export_dir_view), index_(index) {}

      [[nodiscard]] reference operator*() const noexcept {
        ensure_current_export();
        return current_export_;
      }

      [[nodiscard]] pointer operator->() const noexcept {
        ensure_current_export();
        return &current_export_;
      }

      iterator& operator=(const iterator& other) noexcept {
        if (this != &other) {
          export_dir_view_ = other.export_dir_view_;
          index_ = other.index_;
          current_export_ = other.current_export_;
          current_export_ready_ = other.current_export_ready_;
        }

        return *this;
      }

      iterator& operator++() noexcept {
        if (export_dir_view_.present() && index_ < export_dir_view_.names_count()) {
          ++index_;
        }

        current_export_ready_ = false;
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

        current_export_ready_ = false;
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
      void ensure_current_export() const noexcept {
        if (current_export_ready_) {
          return;
        }

        if (!export_dir_view_.present() || index_ >= export_dir_view_.names_count()) {
          current_export_ = value_type{};
          current_export_ready_ = true;
          return;
        }

        const auto function_index = export_dir_view_.function_index(index_);
        if (function_index == detail::export_directory_view::npos) {
          current_export_ = value_type{};
          current_export_ready_ = true;
          return;
        }

        const auto export_address = export_dir_view_.address(function_index);

        current_export_ = value_type{
          .name = export_dir_view_.name(index_),
          .address = export_address,
          .module_base = export_dir_view_.module_base(),
        };

        if (export_dir_view_.is_forwarded(export_address)) {
          current_export_.forwarder_string = forwarder_string::parse(export_address.ptr<const char>());
        }

        current_export_ready_ = true;
      }

      detail::export_directory_view export_dir_view_;
      std::size_t index_{0};
      mutable value_type current_export_{};
      mutable bool current_export_ready_{false};
    };

    static_assert(std::bidirectional_iterator<iterator>);

    [[nodiscard]] iterator begin() const noexcept {
      return {export_dir_view_, 0};
    }

    [[nodiscard]] iterator end() const noexcept {
      return {export_dir_view_, size()};
    }

    [[nodiscard]] iterator find(concepts::hash auto export_name) const noexcept {
      return find_by_hashed_name(export_name);
    }

    [[nodiscard]] iterator find(default_hash export_name) const noexcept {
      return find_by_hashed_name(export_name);
    }

    [[nodiscard]] iterator find_if(std::predicate<const typename iterator::value_type&> auto predicate) const {
      if (directory() == nullptr) {
        return end();
      }

      return std::ranges::find_if(*this, predicate);
    }

   private:
    template <typename Hasher>
    [[nodiscard]] iterator find_by_hashed_name(Hasher export_name_hash) const noexcept {
      if (directory() == nullptr) {
        return end();
      }

      for (std::size_t i{}; i < size(); ++i) {
        const char* export_name = export_dir_view_.name(i);
        if (export_name_hash == omni::hash<Hasher>(export_name)) {
          return {export_dir_view_, i};
        }
      }

      return end();
    }

    detail::export_directory_view export_dir_view_;
  };

  static_assert(omni::concepts::export_range<named_exports, omni::default_hash, named_export>);
  static_assert(std::ranges::viewable_range<named_exports>);

} // namespace omni
