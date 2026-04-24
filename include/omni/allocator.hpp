#pragma once

#include <atomic>

#include "omni/address.hpp"
#include "omni/detail/config.hpp"
#include "omni/hash.hpp"
#include "omni/modules.hpp"
#include "omni/status.hpp"

namespace omni {
  namespace detail {
    inline std::atomic<omni::address::value_type> cached_alloc_proc{};
    inline std::atomic<omni::address::value_type> cached_free_proc{};
  } // namespace detail

  namespace mem {
    [[maybe_unused]] constexpr inline std::uint32_t commit = 0x1000;
    [[maybe_unused]] constexpr inline std::uint32_t reserve = 0x2000;
    [[maybe_unused]] constexpr inline std::uint32_t commit_reserve = commit | reserve;
    [[maybe_unused]] constexpr inline std::uint32_t large_commit = commit | reserve | 0x20000000U;
    [[maybe_unused]] constexpr inline std::uint32_t release = 0x8000;

    namespace page {
      [[maybe_unused]] constexpr inline std::uint32_t no_access = 0x00000001;
      [[maybe_unused]] constexpr inline std::uint32_t read_only = 0x00000002;
      [[maybe_unused]] constexpr inline std::uint32_t read_write = 0x00000004;
      [[maybe_unused]] constexpr inline std::uint32_t execute = 0x00000010;
      [[maybe_unused]] constexpr inline std::uint32_t execute_read = 0x00000020;
      [[maybe_unused]] constexpr inline std::uint32_t execute_read_write = 0x00000040;

      [[maybe_unused]] constexpr inline std::uint32_t guard = 0x00000100;
      [[maybe_unused]] constexpr inline std::uint32_t no_cache = 0x00000200;
      [[maybe_unused]] constexpr inline std::uint32_t write_combine = 0x00000400;
    } // namespace page
  } // namespace mem

  // TODO: Make it standard compliant
  template <typename T, std::uint32_t AllocFlags = mem::commit_reserve, std::uint32_t Protect = mem::page::read_write>
  class nt_allocator {
   public:
    using value_type = T;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <class U>
    struct rebind {
      using other = nt_allocator<U, AllocFlags, Protect>;
    };

    template <class U, std::uint32_t AF, std::uint32_t PR>
    explicit constexpr nt_allocator(const nt_allocator<U, AF, PR>&) noexcept: flags_(AF), protect_(PR) {}
    constexpr nt_allocator() noexcept = default;

    template <typename U>
    explicit constexpr nt_allocator(const nt_allocator<U>&) noexcept {}

    [[nodiscard]] pointer allocate(std::size_t n) {
      std::size_t size = n * sizeof(T);
      void* ptr = virtual_alloc(nullptr, size, flags_, protect_);
      if (ptr == nullptr) {
#ifdef OMNI_HAS_EXCEPTIONS
        throw std::bad_alloc();
#else
        std::abort();
#endif
      }
      return static_cast<T*>(ptr);
    }

    [[nodiscard]] pointer allocate(omni::address address, std::size_t n) {
      std::size_t size = n * sizeof(T);
      void* ptr = virtual_alloc(address.ptr(), size, flags_, protect_);
      if (ptr == nullptr) {
#ifdef OMNI_HAS_EXCEPTIONS
        throw std::bad_alloc();
#else
        std::abort();
#endif
      }
      return static_cast<T*>(ptr);
    }

    void deallocate(pointer ptr, std::size_t) noexcept {
      virtual_free(static_cast<void*>(ptr), 0, mem::release);
    }

    friend bool operator==(nt_allocator, nt_allocator) noexcept {
      return true;
    }

    friend bool operator!=(nt_allocator, nt_allocator) noexcept {
      return false;
    }

    using is_always_equal = std::true_type;

   private:
    static void* virtual_alloc(void* address, std::uint64_t allocation_size, std::uint32_t alloc_type, std::uint32_t protect) {
      void* current_process{reinterpret_cast<void*>(-1)};

      auto nt_allocate_mem = omni::address{detail::cached_alloc_proc.load(std::memory_order_acquire)};
      if (!nt_allocate_mem) {
        auto nt_alloc_export = omni::get_export("NtAllocateVirtualMemory", "ntdll.dll");
        detail::cached_alloc_proc.store(nt_alloc_export.address.value(), std::memory_order_release);
        nt_allocate_mem = nt_alloc_export.address;
      }

      if (!nt_allocate_mem) {
        return nullptr;
      }

      const auto allocation_type = alloc_type & 0xFFFFFFC0;

      auto result = nt_allocate_mem.template invoke<omni::status>(current_process,
        &address,
        0ULL,
        &allocation_size,
        allocation_type,
        protect);
      return result.has_value() && result->is_success() ? address : nullptr;
    }

    static bool virtual_free(void* address, std::uint64_t size, std::uint32_t free_type) {
      void* current_process{reinterpret_cast<void*>(-1)};

      auto nt_free_mem = omni::address{detail::cached_free_proc.load(std::memory_order_acquire)};
      if (!nt_free_mem) {
        auto nt_free = omni::get_export("NtFreeVirtualMemory", "ntdll.dll");
        detail::cached_free_proc.store(nt_free.address.value(), std::memory_order_release);
        nt_free_mem = nt_free.address;
      }

      if (!nt_free_mem) {
        return false;
      }

      auto status = nt_free_mem.template invoke<omni::status>(current_process, &address, &size, free_type);
      return status.has_value() && status->is_success();
    }

    std::uint32_t flags_{AllocFlags};
    std::uint32_t protect_{Protect};
  };

  template <typename T>
  using rw_allocator = nt_allocator<T, mem::commit_reserve, mem::page::read_write>;
  template <typename T>
  using rx_allocator = nt_allocator<T, mem::commit_reserve, mem::page::execute_read>;
  template <typename T>
  using rwx_allocator = nt_allocator<T, mem::commit_reserve, mem::page::execute_read_write>;

} // namespace omni
