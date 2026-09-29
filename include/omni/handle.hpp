#pragma once

#include <memory>
#include <utility>

#include "omni/nt_caller.hpp"
#include "omni/status.hpp"
#include "omni/win/function_signatures.hpp"

namespace omni {

  using native_handle = void*;

  namespace detail {
    inline omni::status nt_close(native_handle handle) noexcept {
#ifdef OMNI_HAS_EXCEPTIONS
      try {
#endif
        omni::default_nt_caller<win::nt_close_fn> nt_close{"NtClose"};
        return nt_close.try_invoke(handle).value_or(ntstatus::procedure_not_found);
#ifdef OMNI_HAS_EXCEPTIONS
      } catch (const std::bad_alloc&) {
        return omni::ntstatus::no_memory;
      } catch (...) {
        return omni::ntstatus::unsuccessful;
      }
#endif
    }
  } // namespace detail

  class unique_handle {
   public:
    using handle_type = native_handle;

    unique_handle() noexcept = default;
    explicit unique_handle(handle_type handle) noexcept: handle_(handle) {}

    unique_handle(const unique_handle&) = delete;
    unique_handle(unique_handle&& other) noexcept: handle_(other.release()) {}

    unique_handle& operator=(const unique_handle&) = delete;
    unique_handle& operator=(unique_handle&& other) noexcept {
      if (this != std::addressof(other)) {
        reset(other.release());
      }

      return *this;
    }

    ~unique_handle() noexcept {
      reset();
    }

    [[nodiscard]] handle_type get() const noexcept {
      return handle_;
    }

    [[nodiscard]] handle_type* out_ptr() noexcept {
      reset();
      return std::addressof(handle_);
    }

    [[nodiscard]] handle_type release() noexcept {
      return std::exchange(handle_, nullptr);
    }

    void reset(handle_type handle = nullptr) noexcept {
      if (handle_ == handle) {
        return;
      }

      const handle_type previous = std::exchange(handle_, handle);
      if (is_valid(previous)) {
        omni::detail::nt_close(previous);
      }
    }

    [[nodiscard]] omni::status close() noexcept {
      if (!valid()) {
        return omni::ntstatus::success;
      }

      const omni::status result = omni::detail::nt_close(handle_);
      if (result == omni::ntstatus::success) {
        handle_ = nullptr;
      }

      return result;
    }

    [[nodiscard]] bool valid() const noexcept {
      return is_valid(handle_);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return valid();
    }

    void swap(unique_handle& other) noexcept {
      std::swap(handle_, other.handle_);
    }

   private:
    [[nodiscard]] static bool is_valid(const handle_type handle) noexcept {
      return handle != nullptr && handle != reinterpret_cast<handle_type>(-1);
    }

    handle_type handle_{};
  };

  inline void swap(unique_handle& left, unique_handle& right) noexcept {
    left.swap(right);
  }

} // namespace omni
