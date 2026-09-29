#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iterator>
#include <limits>
#include <memory>
#include <new>
#include <ranges>
#include <string_view>
#include <system_error>
#include <utility>

#include "omni/allocator.hpp"
#include "omni/process.hpp"
#include "omni/win/system_module_information.hpp"

namespace omni {

  class kernel_module {
   public:
    [[nodiscard]] void* base() const noexcept {
      return info_->image_base;
    }

    [[nodiscard]] std::uint32_t size() const noexcept {
      return info_->image_size;
    }

    [[nodiscard]] std::uint32_t flags() const noexcept {
      return info_->flags;
    }

    [[nodiscard]] std::uint16_t load_order_index() const noexcept {
      return info_->load_order_index;
    }

    [[nodiscard]] std::uint16_t load_count() const noexcept {
      return info_->load_count;
    }

    // Full path as reported by the kernel, e.g. "\SystemRoot\system32\ntoskrnl.exe".
    [[nodiscard]] std::string_view path() const noexcept {
      return {info_->full_path_name, bounded_length(0)};
    }

    // File name component of the path, e.g. "ntoskrnl.exe".
    [[nodiscard]] std::string_view name() const noexcept {
      const std::size_t offset =
        (std::min)(static_cast<std::size_t>(info_->offset_to_file_name), sizeof(info_->full_path_name));
      return {info_->full_path_name + offset, bounded_length(offset)};
    }

    [[nodiscard]] const win::system_module_information& info() const noexcept {
      return *info_;
    }

   private:
    friend class kernel_modules;
    explicit kernel_module(const win::system_module_information* info) noexcept: info_(info) {
      assert(info_ != nullptr);
    }

    [[nodiscard]] std::size_t bounded_length(std::size_t offset) const noexcept {
      const std::string_view tail{info_->full_path_name + offset, sizeof(info_->full_path_name) - offset};
      const auto terminator = tail.find('\0');
      return terminator == std::string_view::npos ? tail.size() : terminator;
    }

    const win::system_module_information* info_;
  };

  class kernel_modules {
   public:
    using allocator_type = omni::nt_allocator<std::byte, mem::commit_reserve, mem::page::read_write>;

    class iterator {
     public:
      using value_type = kernel_module;
      using difference_type = std::ptrdiff_t;
      using iterator_concept = std::forward_iterator_tag;

      iterator() noexcept = default;

      [[nodiscard]] kernel_module operator*() const noexcept {
        return kernel_module{current_};
      }

      iterator& operator++() noexcept {
        ++current_;
        return *this;
      }

      iterator operator++(int) noexcept {
        const iterator previous = *this;
        ++*this;
        return previous;
      }

      [[nodiscard]] friend bool operator==(const iterator&, const iterator&) noexcept = default;

     private:
      friend class kernel_modules;
      explicit iterator(const win::system_module_information* current) noexcept: current_{current} {}

      const win::system_module_information* current_{nullptr};
    };

    static_assert(std::forward_iterator<kernel_modules::iterator>);

    [[nodiscard]] static std::expected<kernel_modules, std::error_code> snapshot() noexcept {
#ifdef OMNI_HAS_EXCEPTIONS
      try {
#endif
        allocator_type allocator;
        detail::process_query_caller query_system_information{"NtQuerySystemInformation"};

        constexpr std::uint32_t system_module_information_class = 11U;
        constexpr std::size_t max_attempts = 8;

        std::uint32_t required_size{};
        auto sizing_result = query_system_information.try_invoke(system_module_information_class, nullptr, 0U, &required_size);
        if (!sizing_result) {
          return std::unexpected(sizing_result.error());
        }
        if (!sizing_result->is_success() && !detail::buffer_too_small(*sizing_result)) {
          return std::unexpected(make_error_code(*sizing_result));
        }

        // A driver may be loaded between the sizing call and the real query,
        // so allocate with a margin
        constexpr std::uint32_t allocation_margin = 16U * 1024U;
        std::uint32_t buffer_size = required_size + allocation_margin;

        buffer_ptr storage;

        for (std::size_t attempt{}; attempt < max_attempts; ++attempt) {
          storage.reset(allocator.allocate(buffer_size));
          required_size = 0U;
          auto result =
            query_system_information.try_invoke(system_module_information_class, storage.get(), buffer_size, &required_size);
          if (!result) {
            return std::unexpected(result.error());
          }
          if (result->is_success()) {
            return kernel_modules{std::move(storage), buffer_size};
          }
          if (!detail::buffer_too_small(*result)) {
            return std::unexpected(make_error_code(*result));
          }

          storage.reset();
          if (buffer_size > (std::numeric_limits<std::uint32_t>::max)() / 2U) {
            return std::unexpected(make_error_code(omni::ntstatus::buffer_too_small));
          }
          buffer_size = (std::max)(required_size, buffer_size * 2U);
        }

        return std::unexpected(make_error_code(omni::ntstatus::info_length_mismatch));
#ifdef OMNI_HAS_EXCEPTIONS
      } catch (const std::bad_alloc&) {
        return std::unexpected(make_error_code(omni::ntstatus::no_memory));
      } catch (...) {
        return std::unexpected(make_error_code(omni::ntstatus::unsuccessful));
      }
#endif
    }

    kernel_modules(kernel_modules&&) noexcept = default;
    kernel_modules& operator=(kernel_modules&&) noexcept = default;
    kernel_modules(const kernel_modules&) = delete;
    kernel_modules& operator=(const kernel_modules&) = delete;
    ~kernel_modules() = default;

    [[nodiscard]] std::size_t size() const noexcept {
      return count_;
    }

    [[nodiscard]] bool empty() const noexcept {
      return count_ == 0;
    }

    [[nodiscard]] iterator begin() const noexcept {
      if (!storage_ || count_ == 0) {
        return end();
      }
      return iterator{first_module()};
    }

    [[nodiscard]] iterator end() const noexcept {
      if (!storage_ || count_ == 0) {
        return iterator{nullptr};
      }
      return iterator{first_module() + count_};
    }

   private:
    struct virtual_free {
      void operator()(std::byte* p) const noexcept {
        if (p == nullptr) {
          return;
        }
        allocator_type allocator;
        allocator.deallocate(p, 0);
      }
    };
    using buffer_ptr = std::unique_ptr<std::byte, virtual_free>;

    kernel_modules(buffer_ptr storage, std::uint32_t buffer_size) noexcept: storage_(std::move(storage)) {
      const auto* header = reinterpret_cast<const win::system_modules_information*>(storage_.get());
      constexpr std::size_t header_size = offsetof(win::system_modules_information, modules);
      // Never trust the reported count beyond what actually fits in the buffer.
      const std::size_t capacity =
        buffer_size < header_size ? 0 : (buffer_size - header_size) / sizeof(win::system_module_information);
      count_ = (std::min)(static_cast<std::size_t>(header->number_of_modules), capacity);
    }

    [[nodiscard]] const win::system_module_information* first_module() const noexcept {
      return reinterpret_cast<const win::system_modules_information*>(storage_.get())->modules;
    }

    buffer_ptr storage_;
    std::size_t count_{};
  };

  static_assert(std::ranges::forward_range<kernel_modules>);

} // namespace omni
