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
#include "omni/detail/start_lifetime_as.hpp"
#include "omni/nt_caller.hpp"
#include "omni/win/function_signatures.hpp"
#include "omni/win/system_module_information.hpp"

namespace omni {

  class kernel_module {
   public:
    [[nodiscard]] omni::address base_address() const noexcept {
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
      return {std::data(info_->full_path_name), bounded_length(0)};
    }

    // File name component of the path, e.g. "ntoskrnl.exe".
    [[nodiscard]] std::string_view name() const noexcept {
      const std::size_t offset =
        (std::min)(static_cast<std::size_t>(info_->offset_to_file_name), sizeof(info_->full_path_name));
      return {std::data(info_->full_path_name) + offset, bounded_length(offset)};
    }

    [[nodiscard]] const win::system_module_information& info() const noexcept {
      return *info_;
    }

   private:
    friend class kernel_modules;

    kernel_module() noexcept = default;
    explicit kernel_module(const win::system_module_information* info) noexcept: info_(info) {
      assert(info_ != nullptr);
    }

    [[nodiscard]] friend bool operator==(const kernel_module&, const kernel_module&) noexcept = default;

    [[nodiscard]] std::size_t bounded_length(std::size_t offset) const noexcept {
      const std::string_view tail{std::data(info_->full_path_name) + offset, sizeof(info_->full_path_name) - offset};
      const auto terminator = tail.find('\0');
      return terminator == std::string_view::npos ? tail.size() : terminator;
    }

    const win::system_module_information* info_{nullptr};
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

      [[nodiscard]] const kernel_module& operator*() const noexcept {
        assert(current_.info_ != nullptr);
        return current_;
      }

      [[nodiscard]] const kernel_module* operator->() const noexcept {
        assert(current_.info_ != nullptr);
        return &current_;
      }

      iterator& operator++() noexcept {
        if (--remaining_ == 0) {
          current_ = kernel_module{};
          return *this;
        }

        const auto* next_location = reinterpret_cast<const std::byte*>(current_.info_) + sizeof(win::system_module_information);
        current_.info_ = detail::start_lifetime_as<win::system_module_information>(next_location);
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
      explicit iterator(const win::system_module_information* current, std::size_t remaining) noexcept
        : current_{current}, remaining_{remaining} {}

      kernel_module current_;
      std::size_t remaining_{};
    };

    static_assert(std::forward_iterator<kernel_modules::iterator>);

    [[nodiscard]] static std::expected<kernel_modules, std::error_code> snapshot() noexcept {
#ifdef OMNI_HAS_EXCEPTIONS
      try {
#endif
        allocator_type allocator;
        omni::default_nt_caller<win::nt_query_system_information_fn> query_system_information{"NtQuerySystemInformation"};

        constexpr std::uint32_t system_module_information_class = 11U;
        constexpr std::size_t max_attempts = 8;

        std::uint32_t required_size{};
        auto sizing_result = query_system_information.try_invoke(system_module_information_class, nullptr, 0U, &required_size);
        if (!sizing_result) {
          return std::unexpected(sizing_result.error());
        }
        if (!sizing_result->is_success() && !buffer_too_small(*sizing_result)) {
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
          if (!buffer_too_small(*result)) {
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

    kernel_modules(kernel_modules&& other) noexcept
      : storage_(std::move(other.storage_)), count_(std::exchange(other.count_, 0U)) {}

    kernel_modules& operator=(kernel_modules&& other) noexcept {
      if (this == &other) {
        return *this;
      }

      storage_ = std::move(other.storage_);
      count_ = std::exchange(other.count_, 0U);
      return *this;
    }

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

      return iterator{detail::start_lifetime_as<win::system_module_information>(first_module()), count_};
    }

    [[nodiscard]] iterator end() const noexcept {
      return iterator{};
    }

   private:
    [[nodiscard]] static bool buffer_too_small(omni::status status) noexcept {
      return status == omni::ntstatus::info_length_mismatch || status == omni::ntstatus::buffer_too_small;
    }

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
      const auto* header = detail::start_lifetime_as<win::system_modules_information>(storage_.get());
      constexpr std::size_t header_size = offsetof(win::system_modules_information, modules);
      // Never trust the reported count beyond what actually fits in the buffer.
      const std::size_t capacity =
        buffer_size < header_size ? 0 : (buffer_size - header_size) / sizeof(win::system_module_information);
      count_ = (std::min)(static_cast<std::size_t>(header->number_of_modules), capacity);
    }

    [[nodiscard]] const std::byte* first_module() const noexcept {
      return storage_.get() + offsetof(win::system_modules_information, modules);
    }

    buffer_ptr storage_;
    std::size_t count_{};
  };

  static_assert(std::ranges::forward_range<kernel_modules>);

} // namespace omni
