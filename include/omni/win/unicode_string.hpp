#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>

namespace omni::win {

  struct unicode_string {
    using char_type = wchar_t;
    using pointer_type = char_type*;

    constexpr unicode_string() = default;
    constexpr unicode_string(pointer_type buffer, std::uint16_t length, std::uint16_t max_length = 0) noexcept
      : length_(length), max_length_(max_length), buffer_(buffer) {}

    unicode_string(const unicode_string& instance) = default;
    unicode_string(unicode_string&& instance) = default;
    unicode_string& operator=(const unicode_string& instance) = default;
    unicode_string& operator=(unicode_string&& instance) = default;
    ~unicode_string() = default;

    [[nodiscard]] auto to_path(std::filesystem::path::format fmt = std::filesystem::path::auto_format) const {
      return std::filesystem::path{view(), fmt};
    }

    [[nodiscard]] std::string string() const {
      auto source_str = std::wstring_view{*this};

      if (std::ranges::all_of(source_str, is_in_ascii_range)) {
        std::string result(source_str.size(), '\0');
        std::ranges::transform(source_str, result.begin(), [](wchar_t ch) { return static_cast<char>(ch); });
        return result;
      }

      std::u8string utf8_str = to_path().u8string();
      return {reinterpret_cast<const char*>(utf8_str.data()), utf8_str.size()};
    }

    [[nodiscard]] std::wstring_view view() const noexcept {
      return std::wstring_view{buffer_, length_ / sizeof(wchar_t)};
    }

    [[nodiscard]] pointer_type data() const noexcept {
      return buffer_;
    }

    [[nodiscard]] std::uint16_t size() const noexcept {
      return length_;
    }

    [[nodiscard]] bool empty() const noexcept {
      return buffer_ == nullptr || length_ == 0;
    }

    [[nodiscard]] bool operator==(const unicode_string& right) const noexcept {
      return buffer_ == right.buffer_ && length_ == right.length_;
    }

    [[nodiscard]] bool operator==(std::wstring_view right) const noexcept {
      return view() == right;
    }

    [[nodiscard]] bool operator==(std::string_view right) const noexcept {
      return std::ranges::equal(view(), right, [](wchar_t left, char right) {
        return is_in_ascii_range(left) and static_cast<char>(left) == right;
      });
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return buffer_ != nullptr;
    }

    [[nodiscard]] explicit operator std::wstring_view() const noexcept {
      return view();
    }

    friend std::wostream& operator<<(std::wostream& os, const unicode_string& unicode_str) {
      return os << unicode_str.view();
    }

   private:
    static bool is_in_ascii_range(wchar_t ch) noexcept {
      return ch <= 127;
    }

    std::uint16_t length_{0};
    [[maybe_unused]] std::uint16_t max_length_{0};
    pointer_type buffer_{nullptr};
  };

} // namespace omni::win
