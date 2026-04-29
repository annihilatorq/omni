#pragma once

#include <iterator>
#include <ranges>

#include "omni/address.hpp"
#include "omni/concepts/concepts.hpp"
#include "omni/hash.hpp"
#include "omni/module.hpp"
#include "omni/module_export.hpp"
#include "omni/win/peb.hpp"

namespace omni {

  class modules {
   public:
    modules() {
      auto* entry = &win::PEB::ptr()->loader_data->in_load_order_module_list;
      begin_ = entry->forward_link;
      end_ = entry;
    }

    modules& skip(std::size_t count = 1) & {
      for (std::size_t i{}; i < count; ++i) {
        if (begin_ != end_) {
          begin_ = begin_->forward_link;
        }
      }
      return *this;
    }

    modules skip(std::size_t count = 1) && {
      auto copy = *this;
      copy.skip(count);
      return copy;
    }

    class iterator {
     public:
      using iterator_category = std::bidirectional_iterator_tag;
      using value_type = omni::module;
      using difference_type = std::ptrdiff_t;
      using pointer = const value_type*;
      using reference = const value_type&;

      iterator() noexcept = default;
      ~iterator() = default;
      iterator(const iterator&) = default;
      iterator(iterator&&) = default;
      iterator& operator=(iterator&&) = default;

      iterator(win::list_entry* entry, win::list_entry* sentinel) noexcept: list_entry_(entry), sentinel_(sentinel) {
        update_current_module();
      }

      [[nodiscard]] reference operator*() const noexcept {
        return current_module_;
      }

      [[nodiscard]] pointer operator->() const noexcept {
        return &current_module_;
      }

      iterator& operator=(const iterator& other) noexcept {
        if (this != &other) {
          list_entry_ = other.list_entry_;
          sentinel_ = other.sentinel_;
          update_current_module();
        }
        return *this;
      }

      iterator& operator++() noexcept {
        list_entry_ = list_entry_->forward_link;
        update_current_module();
        return *this;
      }

      iterator operator++(int) noexcept {
        iterator temp = *this;
        std::ignore = ++(*this);
        return temp;
      }

      iterator& operator--() noexcept {
        list_entry_ = list_entry_->backward_link;
        update_current_module();
        return *this;
      }

      iterator operator--(int) noexcept {
        iterator temp = *this;
        std::ignore = --(*this);
        return temp;
      }

      [[nodiscard]] bool operator==(const iterator& other) const noexcept {
        return list_entry_ == other.list_entry_;
      }

      [[nodiscard]] bool operator!=(const iterator& other) const noexcept {
        return !(*this == other);
      }

     private:
      void update_current_module() noexcept {
        if (list_entry_ == nullptr || list_entry_ == sentinel_) {
          current_module_ = value_type{};
          return;
        }

        auto* table_entry = win::export_containing_record(list_entry_, &win::loader_table_entry::in_load_order_links);

        current_module_ = value_type{table_entry};
      }

      win::list_entry* list_entry_{nullptr};
      win::list_entry* sentinel_{nullptr};
      mutable value_type current_module_;
    };

    static_assert(std::bidirectional_iterator<iterator>);

    [[nodiscard]] iterator begin() const noexcept {
      return {begin_, end_};
    }

    [[nodiscard]] iterator end() const noexcept {
      return {end_, end_};
    }

    [[nodiscard]] iterator find(concepts::hash auto module_name) const {
      return std::ranges::find_if(*this,
        [module_name](const iterator::value_type& module_entry) { return module_entry.matches_name_hash(module_name); });
    }

    [[nodiscard]] iterator find(default_hash module_name) const {
      return find<default_hash>(module_name);
    }

    [[nodiscard]] iterator find(omni::address base_address) const {
      return std::ranges::find_if(*this,
        [base_address](const iterator::value_type& module_entry) { return module_entry.base_address() == base_address; });
    }

    [[nodiscard]] iterator find_if(std::predicate<typename iterator::value_type> auto predicate) const {
      return std::ranges::find_if(*this, predicate);
    }

    [[nodiscard]] bool contains(concepts::hash auto module_name) const {
      // NOLINTNEXTLINE(readability-container-contains)
      return find(module_name) != end();
    }

    [[nodiscard]] bool contains(default_hash module_name) const {
      // NOLINTNEXTLINE(readability-container-contains)
      return contains<default_hash>(module_name);
    }

    [[nodiscard]] bool contains(omni::address base_address) const {
      // NOLINTNEXTLINE(readability-container-contains)
      return find(base_address) != end();
    }

   private:
    win::list_entry* begin_{nullptr};
    win::list_entry* end_{nullptr};
  };

  static_assert(std::ranges::viewable_range<modules>);

  inline omni::module base_module();

  // Overloads to find a loaded module

  inline omni::module get_module(concepts::hash auto module_name);
  inline omni::module get_module(default_hash module_name);
  inline omni::module get_module(omni::address base_address);

  // Overloads to find an export in loaded module(s) EAT

  inline named_export get_export(concepts::hash auto export_name, omni::module module);
  inline named_export get_export(default_hash export_name, omni::module module);

  template <concepts::hash Hasher>
  inline named_export get_export(Hasher export_name, Hasher module_name);
  inline named_export get_export(default_hash export_name, default_hash module_name);

  inline named_export get_export(concepts::hash auto export_name);
  inline named_export get_export(default_hash export_name);

  template <concepts::hash Hasher>
  inline ordinal_export get_export(std::uint32_t ordinal, omni::module module, omni::use_ordinal_t);
  inline ordinal_export get_export(std::uint32_t ordinal, omni::module module, omni::use_ordinal_t);
  inline ordinal_export get_export(std::uint32_t ordinal, concepts::hash auto module_name, omni::use_ordinal_t);
  inline ordinal_export get_export(std::uint32_t ordinal, default_hash module_name, omni::use_ordinal_t);

} // namespace omni

#include "omni/impl/modules.ipp"
