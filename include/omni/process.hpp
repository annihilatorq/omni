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
#include "omni/handle.hpp"
#include "omni/nt_caller.hpp"
#include "omni/win/function_signatures.hpp"
#include "omni/win/object.hpp"
#include "omni/win/system_process_information.hpp"

namespace omni {

  namespace detail {

    [[nodiscard]] inline bool buffer_too_small(omni::status status) noexcept {
      return status == omni::ntstatus::info_length_mismatch || status == omni::ntstatus::buffer_too_small;
    }

  } // namespace detail

  // https://learn.microsoft.com/windows/win32/procthread/process-security-and-access-rights
  enum class process_access : std::uint32_t {
    terminate = 0x0001,
    create_thread = 0x0002,
    set_session_id = 0x0004,
    vm_operation = 0x0008,
    vm_read = 0x0010,
    vm_write = 0x0020,
    dup_handle = 0x0040,
    create_process = 0x0080,
    set_quota = 0x0100,
    set_information = 0x0200,
    query_information = 0x0400,
    suspend_resume = 0x0800,
    query_limited_information = 0x1000,
    set_limited_information = 0x2000,

    // `delete` is reserved by C++, this is equivalent of DELETE
    delete_object = 0x00010000,

    read_control = 0x00020000,
    write_dac = 0x00040000,
    write_owner = 0x00080000,
    synchronize = 0x00100000,

    standard_rights_required = 0x000F0000,
    all_access = 0x001FFFFF,
  };

  [[nodiscard]] constexpr process_access operator|(process_access lhs, process_access rhs) noexcept {
    return static_cast<process_access>(std::to_underlying(lhs) | std::to_underlying(rhs));
  }

  [[nodiscard]] constexpr process_access operator&(process_access lhs, process_access rhs) noexcept {
    return static_cast<process_access>(std::to_underlying(lhs) & std::to_underlying(rhs));
  }

  constexpr process_access& operator|=(process_access& lhs, process_access rhs) noexcept {
    return lhs = lhs | rhs;
  }

  class process {
   public:
    [[nodiscard]] std::expected<omni::unique_handle, std::error_code> open_handle(
      process_access access = process_access::query_limited_information) const noexcept {
#ifdef OMNI_HAS_EXCEPTIONS
      try {
#endif
        win::client_id client_id{
          .unique_process = reinterpret_cast<void*>(static_cast<std::uintptr_t>(id())),
          .unique_thread = nullptr,
        };
        win::object_attributes object_attributes{
          .length = sizeof(win::object_attributes),
          .root_directory = nullptr,
          .object_name = nullptr,
          .attributes = 0,
          .security_descriptor = nullptr,
          .security_quality_of_service = nullptr,
        };
        native_handle handle{};

        omni::default_nt_caller<win::nt_open_process_fn> open_process{"NtOpenProcess"};
        auto result = open_process.try_invoke(&handle, std::to_underlying(access), &object_attributes, &client_id);
        if (!result) {
          return std::unexpected(result.error());
        }
        if (!result->is_success()) {
          return std::unexpected(make_error_code(*result));
        }

        return omni::unique_handle{handle};
#ifdef OMNI_HAS_EXCEPTIONS
      } catch (const std::bad_alloc&) {
        return std::unexpected(omni::make_error_code(omni::ntstatus::no_memory));
      } catch (...) {
        return std::unexpected(omni::make_error_code(omni::ntstatus::unsuccessful));
      }
#endif
    }

    [[nodiscard]] std::uint32_t id() const noexcept {
      return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(info_->unique_process_id));
    }

    [[nodiscard]] std::uint32_t parent_id() const noexcept {
      return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(info_->inherited_from_unique_process_id));
    }

    [[nodiscard]] std::uint32_t session_id() const noexcept {
      return info_->session_id;
    }

    [[nodiscard]] std::uint32_t thread_count() const noexcept {
      return info_->number_of_threads;
    }

    [[nodiscard]] std::wstring_view name() const noexcept {
      return info_->image_name.view();
    }

    [[nodiscard]] const win::system_process_information& info() const noexcept {
      return *info_;
    }

   private:
    friend class processes;

    process() noexcept = default;
    explicit process(const win::system_process_information* info) noexcept: info_(info) {}

    [[nodiscard]] friend bool operator==(const process&, const process&) noexcept = default;

    const win::system_process_information* info_;
  };

  class processes {
   public:
    using allocator_type = omni::nt_allocator<std::byte, mem::commit_reserve, mem::page::read_write>;

    class iterator {
     public:
      using value_type = process;
      using difference_type = std::ptrdiff_t;
      using iterator_concept = std::forward_iterator_tag;

      iterator() noexcept = default;

      [[nodiscard]] const process& operator*() const noexcept {
        assert(current_.info_ != nullptr);
        return current_;
      }

      [[nodiscard]] const process* operator->() const noexcept {
        assert(current_.info_ != nullptr);
        return &current_;
      }

      iterator& operator++() noexcept {
        const std::uint32_t offset = current_.info_->next_entry_offset;
        if (offset == 0) {
          current_ = process{};
        } else {
          const auto* next_location = reinterpret_cast<const std::byte*>(current_.info_) + offset;
#if defined(__cpp_lib_start_lifetime_as)
          current_.info_ = std::start_lifetime_as<win::system_process_information>(next_location);
#else
          // Formally UB, but see comment in processes::begin()
          current_.info_ = reinterpret_cast<const win::system_process_information*>(next_location);
#endif
        }
        return *this;
      }

      iterator operator++(int) noexcept {
        const iterator previous = *this;
        ++*this;
        return previous;
      }

      [[nodiscard]] friend bool operator==(const iterator&, const iterator&) noexcept = default;

     private:
      friend class processes;
      explicit iterator(const win::system_process_information* current) noexcept: current_{current} {}

      process current_;
    };

    static_assert(std::forward_iterator<processes::iterator>);

    [[nodiscard]] static std::expected<processes, std::error_code> snapshot() noexcept {
#ifdef OMNI_HAS_EXCEPTIONS
      try {
#endif
        allocator_type allocator;
        omni::default_nt_caller<win::nt_query_system_information_fn> query_system_information{"NtQuerySystemInformation"};

        constexpr std::size_t max_attempts = 8;

        std::uint32_t required_size{};
        auto sizing_result = query_system_information.try_invoke(5U, nullptr, 0U, &required_size);
        if (!sizing_result) {
          return std::unexpected(sizing_result.error());
        }
        if (!sizing_result->is_success() && !detail::buffer_too_small(*sizing_result)) {
          return std::unexpected(make_error_code(*sizing_result));
        }

        // There is a high probability that a new process was spawned between
        // the call to `NtQuerySystemInformation` and the execution of this
        // code, which is why the first attempt will almost always return an
        // `info_length_mismatch` error. Allocate memory with a margin
        constexpr std::uint32_t allocation_margin = 64U * 1024U;
        std::uint32_t buffer_size = required_size + allocation_margin;

        buffer_ptr storage;

        for (std::size_t attempt{}; attempt < max_attempts; ++attempt) {
          storage.reset(allocator.allocate(buffer_size));
          required_size = 0U;
          auto result = query_system_information.try_invoke(5U, storage.get(), buffer_size, &required_size);
          if (!result) {
            return std::unexpected(result.error());
          }
          if (result->is_success()) {
            return processes{std::move(storage)};
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

    processes(processes&&) noexcept = default;
    processes& operator=(processes&&) noexcept = default;
    processes(const processes&) = delete;
    processes& operator=(const processes&) = delete;
    ~processes() = default;

    [[nodiscard]] iterator begin() const noexcept {
      if (!storage_) {
        return end();
      }

#if defined(__cpp_lib_start_lifetime_as)
      return iterator{std::start_lifetime_as<win::system_process_information>(storage_.get())};
#else
      // Formally, dereferencing this result is UB, due to a violation of the
      // C++ object model. An object of type win::system_process_information
      // was never created at the address storage_.get(). However, this is
      // merely a formality, and in practice, all mainstream compilers
      // support this behavior because users need, for example, to be able to
      // read memory buffers owned by the OS, and then reinterpret_cast the
      // underlying data
      return iterator{reinterpret_cast<const win::system_process_information*>(storage_.get())};
#endif
    }

    [[nodiscard]] iterator end() const noexcept {
      return iterator{nullptr};
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

    explicit processes(buffer_ptr storage) noexcept: storage_(std::move(storage)) {}

    buffer_ptr storage_;
  };

  static_assert(std::ranges::forward_range<processes>);

} // namespace omni
