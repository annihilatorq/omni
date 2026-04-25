#pragma once

#include <array>
#include <cstring>

#include "omni/allocator.hpp"

namespace omni::detail {

  template <std::uint32_t Size>
  class shellcode {
   public:
    using storage_type = std::array<std::uint8_t, Size>;

    explicit shellcode(storage_type shellcode) noexcept: shellcode_(shellcode) {}

    shellcode(const shellcode&) = delete;
    shellcode(shellcode&&) = default;
    shellcode& operator=(const shellcode&) = delete;
    shellcode& operator=(shellcode&&) = default;

    ~shellcode() {
      if (memory_ == nullptr) {
        return;
      }

      allocator_.deallocate(memory_, Size);
      memory_ = nullptr;
    }

    void setup() {
      memory_ = allocator_.allocate(Size);
      if (memory_ != nullptr) {
        std::memcpy(memory_, shellcode_.data(), Size);
        shellcode_fn_ = memory_;
      }
    }

    template <std::integral T = std::uint8_t>
    [[nodiscard]] T read(std::size_t index) const noexcept {
      return shellcode_[index];
    }

    template <std::integral T>
    void write(std::size_t index, T value) noexcept {
      *reinterpret_cast<T*>(&shellcode_[index]) = value;
    }

    template <typename T, typename... Args>
      requires(std::is_default_constructible_v<T>)
    [[nodiscard]] T execute(Args&&... args) const noexcept {
      if (!shellcode_fn_) {
        return T{};
      }
      return reinterpret_cast<T(__stdcall*)(Args...)>(shellcode_fn_)(args...);
    }

    template <typename T = void, typename... Args>
      requires(std::is_void_v<T>)
    [[nodiscard]] T execute(Args&&... args) const noexcept {
      if (!shellcode_fn_) {
        return;
      }
      reinterpret_cast<void(__stdcall*)(Args...)>(shellcode_fn_)(args...);
    }

    template <typename T = void, typename PointerT = std::add_pointer_t<T>>
    [[nodiscard]] PointerT ptr() const noexcept {
      return static_cast<PointerT>(shellcode_fn_);
    }

   private:
    void* shellcode_fn_{nullptr};
    std::uint8_t* memory_{nullptr};
    rwx_allocator<std::uint8_t> allocator_;
    std::array<std::uint8_t, Size> shellcode_{};
  };

} // namespace omni::detail
