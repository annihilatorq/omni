#pragma once

#include <concepts>
#include <cstdint>
#include <format>
#include <iostream>
#include <optional>
#include <span>
#include <variant>

#include <concepts>
#include <span>
#include <string_view>
#include <type_traits>

namespace omni::concepts {

  template <typename T> concept arithmetic = std::is_arithmetic_v<T>;
  template <typename T> concept pointer = std::is_pointer_v<T>;
  template <typename T> concept nullpointer = std::is_null_pointer_v<T>;

  template <typename T>
  concept function_pointer =
    std::is_pointer_v<std::remove_cvref_t<T>> && std::is_function_v<std::remove_pointer_t<std::remove_cvref_t<T>>>;

  template <typename T>
  concept hashable = requires(T t) {
    { t.size() } -> std::convertible_to<std::size_t>;
    { t[0] } -> std::convertible_to<typename T::value_type>;
  };

  template <typename T>
  concept hash = requires(T hasher, const char (&ct_string)[1], const wchar_t (&ct_wstring)[1]) {
    { hasher(std::string_view{}) } -> std::same_as<typename T::value_type>;
    { hasher(std::wstring_view{}) } -> std::same_as<typename T::value_type>;
    { hasher(std::span<char>{}) } -> std::same_as<typename T::value_type>;
    { hasher(std::span<wchar_t>{}) } -> std::same_as<typename T::value_type>;

    { hasher.value() } -> std::same_as<typename T::value_type>;

    hasher == hasher;
    hasher == typename T::value_type{};

    T{};
    T{ct_string};
    T{ct_wstring};
    T{std::string_view{}};
    T{typename T::value_type{}};
  };

} // namespace omni::concepts

namespace omni {

  class address {
   public:
    using value_type = std::uintptr_t;
    using difference_type = std::ptrdiff_t;

    constexpr address() = default;

    // This is necessary to avoid ambiguity between `std::nullptr_t` and any
    // integer types other than `std::uintptr_t`. To convert `int` (0 & NULL)
    // to `uintptr_t`, the compiler needs a single conversion. In the case of
    // std::nullptr_t, the compiler also needs a single conversion to turn int
    // into std::nullptr_t. In both cases, a single conversion is required, none
    // of the standard conversion sequences is shorter than the other -> ambiguity.
    // This ambiguity could be avoided by requiring all library users to pass
    // specifically uintptr_t (x64 - 0ULL, x86 - 0UL), which would create a
    // perfect-match function signature, but would be pretty inconvenient.
    constexpr explicit address(concepts::nullpointer auto) noexcept {}

    constexpr explicit(false) address(value_type address) noexcept: address_(address) {}
    constexpr explicit address(concepts::pointer auto ptr) noexcept: address_(reinterpret_cast<value_type>(ptr)) {}
    constexpr explicit address(std::ranges::contiguous_range auto range) noexcept
      : address_(reinterpret_cast<value_type>(range.data())) {}

    address(const address&) = default;
    address(address&&) = default;

    address& operator=(const address&) = default;
    address& operator=(address&&) = default;

    address& operator=(std::nullptr_t) {
      address_ = 0;
      return *this;
    }

    address& operator=(concepts::pointer auto ptr) {
      address_ = reinterpret_cast<value_type>(ptr);
      return *this;
    }

    ~address() = default;

    template <typename T = void, typename PointerT = std::add_pointer_t<T>>
    [[nodiscard]] constexpr PointerT ptr(difference_type offset = 0) const noexcept {
      return this->offset(offset).as<PointerT>();
    }

    [[nodiscard]] constexpr value_type value() const noexcept {
      return address_;
    }

    template <concepts::pointer T>
    [[nodiscard]] constexpr T offset(difference_type offset = 0) const noexcept {
      return address_ == 0U ? nullptr : reinterpret_cast<T>(address_ + offset);
    }

    template <typename T = address>
    [[nodiscard]] constexpr T offset(difference_type offset = 0) const noexcept {
      return address_ == 0U ? static_cast<T>(*this) : T{address_ + offset};
    }

    template <concepts::pointer T>
    [[nodiscard]] constexpr T as() const noexcept {
      return reinterpret_cast<T>(address_);
    }

    template <std::convertible_to<value_type> T>
    [[nodiscard]] constexpr T as() const noexcept {
      return static_cast<T>(address_);
    }

    template <typename T, std::size_t Extent = std::dynamic_extent>
    [[nodiscard]] constexpr std::span<T, Extent> span(std::size_t count) const noexcept {
      return {this->ptr<T>(), count};
    }

    [[nodiscard]] bool is_in_range(address start, address end) const noexcept {
      return (*this >= start) && (*this < end);
    }

    template <typename T = std::monostate, typename... Args>
    [[nodiscard]] auto invoke(Args&&... args) const noexcept {
      using target_function_t = T(__stdcall*)(std::decay_t<Args>...);

      if (address_ == 0) {
        if constexpr (std::is_void_v<T>) {
          return false;
        } else {
          return std::optional<T>{};
        }
      }

      const auto target_function = reinterpret_cast<target_function_t>(address_);

      if constexpr (std::is_void_v<T>) {
        target_function(std::forward<Args>(args)...);
        return true;
      } else {
        return std::optional<T>{target_function(std::forward<Args>(args)...)};
      }
    }

    constexpr explicit operator std::uintptr_t() const noexcept {
      return address_;
    }

    constexpr explicit operator bool() const noexcept {
      return static_cast<bool>(address_);
    }

    constexpr auto operator<=>(const address&) const = default;

    [[nodiscard]] bool operator==(const address& other) const noexcept {
      return address_ == other.address_;
    }

    [[nodiscard]] bool operator==(concepts::pointer auto ptr) const noexcept {
      return *this == address{ptr};
    }

    [[nodiscard]] bool operator==(concepts::nullpointer auto) const noexcept {
      return address_ == 0;
    }

    [[nodiscard]] bool operator==(value_type value) const noexcept {
      return address_ == value;
    }

    constexpr address& operator+=(const address& rhs) noexcept {
      address_ += rhs.address_;
      return *this;
    }

    constexpr address& operator-=(const address& rhs) noexcept {
      address_ -= rhs.address_;
      return *this;
    }

    [[nodiscard]] constexpr address operator+(const address& rhs) const noexcept {
      return address{address_ + rhs.address_};
    }

    [[nodiscard]] constexpr address operator-(const address& rhs) const noexcept {
      return address{address_ - rhs.address_};
    }

    [[nodiscard]] constexpr address operator&(const address& other) const noexcept {
      return address{address_ & other.address_};
    }

    [[nodiscard]] constexpr address operator|(const address& other) const noexcept {
      return address{address_ | other.address_};
    }

    [[nodiscard]] constexpr address operator^(const address& other) const noexcept {
      return address{address_ ^ other.address_};
    }

    [[nodiscard]] constexpr address operator<<(std::size_t shift) const noexcept {
      return address{address_ << shift};
    }

    [[nodiscard]] constexpr address operator>>(std::size_t shift) const noexcept {
      return address{address_ >> shift};
    }

    friend std::ostream& operator<<(std::ostream& os, const address& address) {
      return os << address.ptr();
    }

   private:
    value_type address_{0};
  };

} // namespace omni

template <>
struct std::formatter<omni::address> : std::formatter<omni::address::value_type> {
  auto format(const omni::address& address, std::format_context& ctx) const {
    return std::formatter<omni::address::value_type>::format(address.value(), ctx);
  }
};

#include <atomic>

#if defined(__clang__)
#  define OMNI_COMPILER_CLANG
#  if defined(_MSC_VER)
#    define OMNI_COMPILER_MSVC_COMPAT
#  endif
#elif defined(_MSC_VER)
#  define OMNI_COMPILER_MSVC
#  define OMNI_COMPILER_MSVC_COMPAT
#elif defined(__GNUC__)
#  define OMNI_COMPILER_GCC
#endif

#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
#  define OMNI_ARCH_X64
#elif defined(_M_IX86) || defined(__i386__)
#  define OMNI_ARCH_X86
#endif

#if !defined(OMNI_DISABLE_EXCEPTIONS) || defined(__cpp_exceptions) || defined(_CPPUNWIND)
#  define OMNI_HAS_EXCEPTIONS
#endif

#if !defined(OMNI_HAS_CACHING)
#  if !defined(OMNI_DISABLE_CACHING)
#    define OMNI_HAS_CACHING
#  endif
#endif

#if !defined(OMNI_DISABLE_ERROR_STRINGS)
#  if defined(OMNI_ENABLE_ERROR_STRINGS) || defined(DEBUG) || defined(_DEBUG)
#    define OMNI_HAS_ERROR_STRINGS
#  endif
#endif

namespace omni::detail {
#ifdef OMNI_ARCH_X64
  [[maybe_unused]] constexpr inline bool is_x64 = true;
#else
  [[maybe_unused]] constexpr inline bool is_x64 = false;
#endif

#ifdef OMNI_ARCH_X86
  [[maybe_unused]] constexpr inline bool is_x86 = true;
#else
  [[maybe_unused]] constexpr inline bool is_x86 = false;
#endif
} // namespace omni::detail

#include <cstdint>
#include <format>

namespace omni::detail {

  template <std::unsigned_integral T>
  class fnv1a_hash {
    constexpr static auto FNV_prime = (sizeof(T) == 4) ? 16777619ULL : 1099511628211ULL;
    constexpr static auto FNV_offset_basis = (sizeof(T) == 4) ? 2166136261ULL : 14695981039346656037ULL;

   public:
    constexpr static auto initial_value = FNV_offset_basis;
    using value_type = T;

    constexpr fnv1a_hash() = default;

    constexpr explicit(false) fnv1a_hash(value_type value): value_(value) {}

    // Implicit constructor is key here. It allows passing string literals in
    // parameter-list where basic_hash type is expected, using this implicit
    // consteval constructor we perform compile-time string hashing without
    // forcing user to specify hash type, so instead of writing this:
    // `foo(omni::hash_name{"string"})`
    // user will need to write:
    // `foo("string")`
    // while still achieving absolutely the same result and still being able
    // to specify their own hashing policy
    template <typename CharT, std::size_t N>
    consteval explicit(false) fnv1a_hash(const CharT (&string)[N]) {
      for (std::size_t i{}; i < N - 1; i++) {
        value_ = fnv1a_append_bytes<CharT>(value_, string[i]);
      }
    }

    consteval explicit(false) fnv1a_hash(concepts::hashable auto string) {
      for (std::size_t i{}; i < string.size(); i++) {
        value_ = fnv1a_append_bytes<>(value_, string[i]);
      }
    }

    [[nodiscard]] value_type operator()(concepts::hashable auto object) {
      T value{FNV_offset_basis};
      for (std::size_t i{}; i < object.size(); i++) {
        value = fnv1a_append_bytes<>(value, object[i]);
      }
      return value;
    }

    [[nodiscard]] value_type value() const {
      return value_;
    }

    [[nodiscard]] constexpr auto operator<=>(const fnv1a_hash&) const = default;

    [[nodiscard]] constexpr auto operator<=>(value_type other) const {
      return value_ <=> other;
    }

    [[nodiscard]] constexpr bool operator==(value_type other) const {
      return value_ == other;
    }

    [[nodiscard]] constexpr bool operator==(const fnv1a_hash& other) const {
      return value_ == other.value_;
    }

    friend std::ostream& operator<<(std::ostream& os, const fnv1a_hash& hash) {
      return os << hash.value();
    }

   private:
    template <typename CharT>
    [[nodiscard]] constexpr static value_type fnv1a_append_bytes(value_type accumulator, CharT byte) {
      accumulator ^= static_cast<value_type>(to_lower(byte));
      accumulator *= FNV_prime;
      return accumulator;
    }

    template <typename CharT>
    [[nodiscard]] constexpr static CharT to_lower(CharT c) {
      return ((c >= 'A' && c <= 'Z') ? (c + 32) : c);
    }

    value_type value_{FNV_offset_basis};
  };

  static_assert(concepts::hash<fnv1a_hash<std::uint32_t>>);
  static_assert(concepts::hash<fnv1a_hash<std::uint64_t>>);

} // namespace omni::detail

namespace omni {

  using fnv1a32 = detail::fnv1a_hash<std::uint32_t>;
  using fnv1a64 = detail::fnv1a_hash<std::uint64_t>;
  using default_hash = fnv1a64;

  template <concepts::hash Hasher = default_hash>
  struct hash_pair {
    consteval hash_pair(Hasher first, Hasher second): first(first), second(second) {}

    Hasher first;
    Hasher second;
  };

  static_assert(concepts::hash<fnv1a32>);
  static_assert(concepts::hash<fnv1a64>);
  static_assert(concepts::hash<default_hash>);

  template <typename T>
  [[nodiscard]] constexpr auto hash(concepts::hashable auto object) {
    return T{}(object);
  }

  namespace literals {
    consteval omni::fnv1a32 operator""_fnv1a32(const char* str, std::size_t len) noexcept {
      return {std::string_view{str, len}};
    }

    consteval omni::fnv1a64 operator""_fnv1a64(const char* str, std::size_t len) noexcept {
      return {std::string_view{str, len}};
    }

    consteval omni::default_hash operator""_hash(const char* str, std::size_t len) noexcept {
      return {std::string_view{str, len}};
    }

    consteval omni::fnv1a32 operator""_fnv1a32(const wchar_t* str, std::size_t len) noexcept {
      return {std::wstring_view{str, len}};
    }

    consteval omni::fnv1a64 operator""_fnv1a64(const wchar_t* str, std::size_t len) noexcept {
      return {std::wstring_view{str, len}};
    }

    consteval omni::default_hash operator""_hash(const wchar_t* str, std::size_t len) noexcept {
      return {std::wstring_view{str, len}};
    }
  } // namespace literals

} // namespace omni

template <>
struct std::formatter<omni::fnv1a32> : std::formatter<omni::fnv1a32::value_type> {
  auto format(const omni::fnv1a32& hash, std::format_context& ctx) const {
    return std::formatter<omni::fnv1a32::value_type>::format(hash.value(), ctx);
  }
};

template <>
struct std::formatter<omni::fnv1a64> : std::formatter<omni::fnv1a64::value_type> {
  auto format(const omni::fnv1a64& hash, std::format_context& ctx) const {
    return std::formatter<omni::fnv1a64::value_type>::format(hash.value(), ctx);
  }
};

#include <iterator>
#include <ranges>

#include <cassert>
#include <filesystem>
#include <format>
#include <iosfwd>
#include <string>
#include <string_view>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <string_view>

#include <array>
#include <cassert>
#include <cstdint>
#include <format>
#include <limits>
#include <string_view>

#include <algorithm>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace omni::win {

  struct api_set_namespace;

  struct api_set_hash_entry {
    std::uint32_t hash;
    std::uint32_t index;
  };

  struct api_set_value_entry {
    std::uint32_t flags;
    std::uint32_t name_offset;
    std::uint32_t name_length;
    std::uint32_t value_offset;
    std::uint32_t value_length;

    [[nodiscard]] std::wstring_view value(omni::address api_set_namespace) const noexcept {
      if (value_length == 0) {
        return {};
      }

      auto* value_string_ptr = api_set_namespace.offset<wchar_t*>(value_offset);
      return {value_string_ptr, value_length / sizeof(wchar_t)};
    }

    [[nodiscard]] std::wstring_view alias(omni::address api_set_namespace) const noexcept {
      if (name_length == 0) {
        return {};
      }

      auto* name_string_ptr = api_set_namespace.offset<wchar_t*>(name_offset);
      return {name_string_ptr, name_length / sizeof(wchar_t)};
    }
  };

  struct api_set_namespace_entry {
    std::uint32_t flags;
    std::uint32_t name_offset;
    std::uint32_t name_length;
    std::uint32_t hashed_length;
    std::uint32_t value_offset;
    std::uint32_t value_count;

    [[nodiscard]] std::span<const api_set_value_entry> value_entries(omni::address api_set_namespace) const noexcept {
      const auto* first_entry = api_set_namespace.offset<const api_set_value_entry*>(value_offset);
      return {first_entry, value_count};
    }

    [[nodiscard]] bool is_sealed() const noexcept {
      return (flags & 1U) != 0;
    }

    [[nodiscard]] std::wstring_view contract_name(omni::address api_set_namespace) const noexcept {
      auto* name_string_ptr = api_set_namespace.offset<wchar_t*>(name_offset);
      return {name_string_ptr, name_length / sizeof(wchar_t)};
    }

    [[nodiscard]] api_set_value_entry* get_value_entry(omni::address api_set_namespace) const noexcept {
      return api_set_namespace.offset<api_set_value_entry*>(value_offset);
    }
  };

  struct api_set_namespace {
    std::uint32_t version;
    std::uint32_t size;
    std::uint32_t flags;
    std::uint32_t entries_count;
    std::uint32_t entry_offset;
    std::uint32_t hash_offset;
    std::uint32_t hash_factor;

    [[nodiscard]] std::span<const api_set_namespace_entry> namespace_entries() const noexcept {
      const auto* first_entry =
        reinterpret_cast<const api_set_namespace_entry*>(reinterpret_cast<const std::byte*>(this) + entry_offset);

      return {first_entry, entries_count};
    }

    [[nodiscard]] api_set_namespace_entry* get_namespace_entry(std::size_t index) const noexcept {
      return omni::address{this}.offset<api_set_namespace_entry*>(entry_offset) + index;
    }
  };

} // namespace omni::win

namespace omni {

  struct api_set_host {
    std::wstring_view value;
    std::wstring_view alias;

    [[nodiscard]] bool present() const noexcept {
      return !value.empty() || !alias.empty();
    }

    [[nodiscard]] bool is_default() const noexcept {
      return alias.empty();
    }
  };

  class api_set_hosts {
   public:
    api_set_hosts() noexcept = default;

    api_set_hosts(std::span<const win::api_set_value_entry> entries, omni::address api_set_map_address) noexcept
      : entries_(entries), api_set_map_address_(api_set_map_address) {}

    class iterator {
     public:
      using iterator_category = std::forward_iterator_tag;
      using value_type = api_set_host;
      using difference_type = std::ptrdiff_t;
      using pointer = const value_type*;
      using reference = const value_type&;

      iterator() noexcept: index_(0), api_set_map_address_(0) {}

      iterator(std::span<const win::api_set_value_entry> entries, omni::address api_set_map_address, std::size_t index = 0)
        : entries_(entries), index_(index), api_set_map_address_(api_set_map_address) {
        update_value();
      }

      [[nodiscard]] reference operator*() const noexcept {
        return current_;
      }

      [[nodiscard]] pointer operator->() const noexcept {
        return &current_;
      }

      iterator& operator++() noexcept {
        if (index_ < entries_.size()) {
          ++index_;
          update_value();
        }
        return *this;
      }

      iterator operator++(int) noexcept {
        iterator tmp = *this;
        ++(*this);
        return tmp;
      }

      bool operator==(const iterator& other) const noexcept {
        return entries_.data() == other.entries_.data() && entries_.size() == other.entries_.size() && index_ == other.index_ &&
               api_set_map_address_ == other.api_set_map_address_;
      }

      bool operator!=(const iterator& other) const noexcept {
        return !(*this == other);
      }

     private:
      void update_value() noexcept {
        if (entries_.empty() || index_ >= entries_.size()) {
          current_ = value_type{};
          return;
        }

        const win::api_set_value_entry& entry = entries_[index_];
        current_ = value_type{
          .value = entry.value(api_set_map_address_),
          .alias = entry.alias(api_set_map_address_),
        };
      }

      std::span<const win::api_set_value_entry> entries_;
      std::size_t index_;
      omni::address api_set_map_address_;
      mutable value_type current_;
    };

    static_assert(std::forward_iterator<iterator>);

    [[nodiscard]] iterator begin() const {
      return {entries_, api_set_map_address_, 0};
    }

    [[nodiscard]] iterator end() const {
      return {entries_, api_set_map_address_, size()};
    }

    [[nodiscard]] std::size_t size() const noexcept {
      return entries_.size();
    }

   private:
    std::span<const win::api_set_value_entry> entries_;
    omni::address api_set_map_address_;
  };

  static_assert(std::ranges::viewable_range<api_set_hosts>);

  class api_set {
   public:
    api_set() = default;

    api_set(std::wstring_view contract_name, bool sealed, std::span<const win::api_set_value_entry> entries,
      omni::address base) noexcept
      : contract_name_(contract_name), is_sealed_(sealed), value_entries_(entries), base_(base) {}

    // Contract, for example: "api-ms-win-core-com-l1-1-0"
    [[nodiscard]] std::wstring_view contract_name() const noexcept {
      return contract_name_;
    }

    [[nodiscard]] bool is_sealed() const noexcept {
      return is_sealed_;
    }

    [[nodiscard]] api_set_hosts hosts() const noexcept {
      return {value_entries_, base_};
    }

    [[nodiscard]] std::optional<api_set_host> default_host() const noexcept {
      return find_host_entry({});
    }

    [[nodiscard]] std::optional<api_set_host> resolve_host(std::wstring_view alias = {}) const noexcept {
      auto exact_host = find_host_entry(alias);
      if (exact_host.has_value()) {
        return exact_host;
      }

      if (alias.empty()) {
        return std::nullopt;
      }

      return default_host();
    }

    [[nodiscard]] bool present() const noexcept {
      return !contract_name_.empty() && base_ != nullptr;
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return present();
    }

   private:
    [[nodiscard]] std::optional<api_set_host> find_host_entry(std::wstring_view alias) const noexcept {
      auto all_hosts = hosts();
      auto host = std::ranges::find_if(all_hosts, [alias](const api_set_host& candidate) { return candidate.alias == alias; });
      if (host == all_hosts.end()) {
        return std::nullopt;
      }

      return *host;
    }

    std::wstring_view contract_name_;
    bool is_sealed_{};

    std::span<const win::api_set_value_entry> value_entries_;
    omni::address base_;
  };

} // namespace omni

namespace omni {

  struct use_ordinal_t {};
  [[maybe_unused]] constexpr inline use_ordinal_t use_ordinal{};

  struct forwarder_string {
    std::string_view module;
    std::string_view function;

    [[nodiscard]] static forwarder_string parse(std::string_view forwarder_str) noexcept {
      auto pos = forwarder_str.find('.');
      if (pos != std::string_view::npos) {
        auto first_part = forwarder_str.substr(0, pos);
        auto second_part = forwarder_str.substr(pos + 1);
        return forwarder_string{.module = first_part, .function = second_part};
      }
      assert(false);
      return forwarder_string{.module = forwarder_str, .function = std::string_view{}};
    }

    [[nodiscard]] bool is_ordinal() const noexcept {
      return !function.empty() && function.front() == '#';
    }

    [[nodiscard]] std::uint32_t to_ordinal() const noexcept {
      if (function.empty()) {
        return 0;
      }

      std::uint32_t ordinal{};
      // Ordinal forwarder always starts from '#', skip it
      auto ordinal_str = function.substr(1);
      [[maybe_unused]] auto result = std::from_chars(ordinal_str.data(), ordinal_str.data() + ordinal_str.size(), ordinal);
      assert(result.ec == std::errc{});
      return ordinal;
    }

    [[nodiscard]] bool present() const noexcept {
      return !(module.empty() || function.empty());
    }
  };

  struct module_export {
    std::string_view name;
    omni::address address;
    std::uint32_t ordinal{};
    bool is_forwarded{};
    omni::forwarder_string forwarder_string{};
    std::optional<omni::api_set> forwarder_api_set;
    omni::address module_base;

    [[nodiscard]] bool is_ordinal_only() const noexcept {
      return name.empty();
    }

    [[nodiscard]] bool present() const noexcept {
      return static_cast<bool>(address);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return present();
    }
  };

} // namespace omni

template <>
struct std::formatter<omni::module_export> : std::formatter<std::string_view> {
  auto format(const omni::module_export& module_export, std::format_context& ctx) const {
    if (!module_export.present()) {
      return std::formatter<std::string_view, char>::format({}, ctx);
    }

    if (!module_export.is_ordinal_only()) {
      return std::formatter<std::string_view, char>::format(module_export.name, ctx);
    }

    // Zero-allocation path for ordinal exports, since the formatter is
    // required to write data from the view to the format_context::out()
    // before exiting the scope
    std::array<char, std::numeric_limits<std::uint32_t>::digits> ordinal_buf{};
    ordinal_buf[0] = '#';

    auto conversion_result =
      std::to_chars(ordinal_buf.data() + 1, ordinal_buf.data() + ordinal_buf.size(), module_export.ordinal);
    std::size_t digits_converted = conversion_result.ptr - ordinal_buf.data();

    std::string_view ordinal_view(ordinal_buf.data(), digits_converted);
    return std::formatter<std::string_view, char>::format(ordinal_view, ctx);
  }
};

#include <cstdint>

#include <cstdint>

namespace omni::win {

  union version {
    std::uint16_t identifier;
    struct {
      std::uint8_t major;
      std::uint8_t minor;
    };
  };

  union ex_version {
    std::uint32_t identifier;
    struct {
      std::uint16_t major;
      std::uint16_t minor;
    };
  };

} // namespace omni::win

namespace omni::win {

  constexpr inline std::uint32_t num_data_directories = 16U;

  enum directory_id : std::uint8_t {
    directory_entry_export = 0,          // Export Directory
    directory_entry_import = 1,          // Import Directory
    directory_entry_resource = 2,        // Resource Directory
    directory_entry_exception = 3,       // Exception Directory
    directory_entry_security = 4,        // Security Directory
    directory_entry_basereloc = 5,       // Base Relocation Table
    directory_entry_debug = 6,           // Debug Directory
    directory_entry_copyright = 7,       // (X86 usage)
    directory_entry_architecture = 7,    // Architecture Specific Data
    directory_entry_globalptr = 8,       // RVA of GP
    directory_entry_tls = 9,             // TLS Directory
    directory_entry_load_config = 10,    // Load Configuration Directory
    directory_entry_bound_import = 11,   // Bound Import Directory in headers
    directory_entry_iat = 12,            // Import Address Table
    directory_entry_delay_import = 13,   // Delay Load Import Descriptors
    directory_entry_com_descriptor = 14, // COM Runtime descriptor
    directory_reserved0 = 15,            // -
  };

  struct data_directory {
    std::uint32_t rva;
    std::uint32_t size;

    [[nodiscard]] bool present() const noexcept {
      return size > 0;
    }
  };

  struct raw_data_directory {
    std::uint32_t ptr_raw_data;
    std::uint32_t size;

    [[nodiscard]] bool present() const noexcept {
      return size > 0;
    }
  };

  struct data_directories_x64 {
    union {
      struct {
        data_directory export_directory;
        data_directory import_directory;
        data_directory resource_directory;
        data_directory exception_directory;
        raw_data_directory security_directory; // File offset instead of RVA!
        data_directory basereloc_directory;
        data_directory debug_directory;
        data_directory architecture_directory;
        data_directory globalptr_directory;
        data_directory tls_directory;
        data_directory load_config_directory;
        data_directory bound_import_directory;
        data_directory iat_directory;
        data_directory delay_import_directory;
        data_directory com_descriptor_directory;
        data_directory _reserved0;
      };
      data_directory entries[num_data_directories];
    };
  };

  struct data_directories_x86 {
    union {
      struct {
        data_directory export_directory;
        data_directory import_directory;
        data_directory resource_directory;
        data_directory exception_directory;
        raw_data_directory security_directory; // File offset instead of RVA!
        data_directory basereloc_directory;
        data_directory debug_directory;
        data_directory copyright_directory;
        data_directory globalptr_directory;
        data_directory tls_directory;
        data_directory load_config_directory;
        data_directory bound_import_directory;
        data_directory iat_directory;
        data_directory delay_import_directory;
        data_directory com_descriptor_directory;
        data_directory _reserved0;
      };
      data_directory entries[num_data_directories];
    };
  };

  struct export_directory {
    std::uint32_t characteristics;
    std::uint32_t timedate_stamp;
    win::version version;
    std::uint32_t name;
    std::uint32_t base;
    std::uint32_t num_functions;
    std::uint32_t num_names;
    std::uint32_t rva_functions;
    std::uint32_t rva_names;
    std::uint32_t rva_name_ordinals;

    [[nodiscard]] auto rva_table(std::uintptr_t base_address) const {
      return reinterpret_cast<std::uint32_t*>(base_address + rva_functions);
    }

    [[nodiscard]] auto ordinal_table(std::uintptr_t base_address) const {
      return reinterpret_cast<std::uint16_t*>(base_address + rva_name_ordinals);
    }
  };

} // namespace omni::win

#include <cstdint>
#include <limits>
#include <memory>

#include <cstdint>

#include <cstdint>

namespace omni::win {

  struct file_header {
    std::uint16_t machine;
    std::uint16_t num_sections;
    std::uint32_t timedate_stamp;
    std::uint32_t ptr_symbols;
    std::uint32_t num_symbols;
    std::uint16_t size_optional_header;
    std::uint16_t characteristics;
  };

} // namespace omni::win

#include <cstdint>
#include <type_traits>

namespace omni::win {

  enum class subsystem_id : std::uint16_t {
    unknown = 0x0000,        // Unknown subsystem.
    native = 0x0001,         // Image doesn't require a subsystem.
    windows_gui = 0x0002,    // Image runs in the Windows GUI subsystem.
    windows_cui = 0x0003,    // Image runs in the Windows character subsystem
    os2_cui = 0x0005,        // image runs in the OS/2 character subsystem.
    posix_cui = 0x0007,      // image runs in the Posix character subsystem.
    native_windows = 0x0008, // image is a native Win9x driver.
    windows_ce_gui = 0x0009, // Image runs in the Windows CE subsystem.
    efi_application = 0x000A,
    efi_boot_service_driver = 0x000B,
    efi_runtime_driver = 0x000C,
    efi_rom = 0x000D,
    xbox = 0x000E,
    windows_boot_application = 0x0010,
    xbox_code_catalog = 0x0011,
  };

  struct optional_header_x64 {
    std::uint16_t magic;
    version linker_version;
    std::uint32_t size_code;
    std::uint32_t size_init_data;
    std::uint32_t size_uninit_data;
    std::uint32_t entry_point;
    std::uint32_t base_of_code;
    std::uint64_t image_base;
    std::uint32_t section_alignment;
    std::uint32_t file_alignment;
    ex_version os_version;
    ex_version img_version;
    ex_version subsystem_version;
    std::uint32_t win32_version_value;
    std::uint32_t size_image;
    std::uint32_t size_headers;
    std::uint32_t checksum;
    subsystem_id subsystem;
    std::uint16_t characteristics;
    std::uint64_t size_stack_reserve;
    std::uint64_t size_stack_commit;
    std::uint64_t size_heap_reserve;
    std::uint64_t size_heap_commit;
    std::uint32_t ldr_flags;
    std::uint32_t num_data_directories;
    data_directories_x64 data_directories;
  };

  struct optional_header_x86 {
    std::uint16_t magic;
    version linker_version;
    std::uint32_t size_code;
    std::uint32_t size_init_data;
    std::uint32_t size_uninit_data;
    std::uint32_t entry_point;
    std::uint32_t base_of_code;
    std::uint32_t base_of_data;
    std::uint32_t image_base;
    std::uint32_t section_alignment;
    std::uint32_t file_alignment;
    ex_version os_version;
    ex_version img_version;
    ex_version subsystem_version;
    std::uint32_t win32_version_value;
    std::uint32_t size_image;
    std::uint32_t size_headers;
    std::uint32_t checksum;
    subsystem_id subsystem;
    std::uint16_t characteristics;
    std::uint32_t size_stack_reserve;
    std::uint32_t size_stack_commit;
    std::uint32_t size_heap_reserve;
    std::uint32_t size_heap_commit;
    std::uint32_t ldr_flags;
    std::uint32_t num_data_directories;
    data_directories_x86 data_directories;

    [[nodiscard]] bool has_directory(const data_directory* dir) const {
      if (dir == nullptr) {
        return false;
      }

      return &data_directories.entries[num_data_directories] < dir && dir->present();
    }

    [[nodiscard]] bool has_directory(directory_id id) const {
      return has_directory(&data_directories.entries[id]);
    }
  };

  using optional_header = std::conditional_t<detail::is_x64, optional_header_x64, optional_header_x86>;

} // namespace omni::win

#include <string_view>

namespace omni::win {

  struct section_name {
    constexpr static auto max_section_name_length{8};

    char name[max_section_name_length];

    [[nodiscard]] explicit operator std::string_view() const noexcept {
      return std::string_view{static_cast<const char*>(name)};
    }

    [[nodiscard]] auto operator[](size_t n) const noexcept {
      return static_cast<std::string_view>(*this)[n];
    }

    [[nodiscard]] bool operator==(const section_name& other) const {
      return std::string_view{*this} == std::string_view{other};
    }
  };

  struct section_header {
    section_name name;
    union {
      std::uint32_t physical_address;
      std::uint32_t virtual_size;
    };
    std::uint32_t virtual_address;

    std::uint32_t size_raw_data;
    std::uint32_t ptr_raw_data;

    std::uint32_t ptr_relocs;
    std::uint32_t ptr_line_numbers;
    std::uint16_t num_relocs;
    std::uint16_t num_line_numbers;

    std::uint32_t characteristics_flags;
  };

} // namespace omni::win

namespace omni::win {

  struct nt_headers {
    std::uint32_t signature;
    win::file_header file_header;
    win::optional_header optional_header;

    [[nodiscard]] section_header* get_sections() {
      return reinterpret_cast<section_header*>(
        reinterpret_cast<std::byte*>(&optional_header) + file_header.size_optional_header);
    }

    [[nodiscard]] section_header* get_section(size_t n) {
      return n >= file_header.num_sections ? nullptr : get_sections() + n;
    }

    [[nodiscard]] const section_header* get_sections() const {
      return reinterpret_cast<const section_header*>(
        reinterpret_cast<const std::byte*>(&optional_header) + file_header.size_optional_header);
    }

    [[nodiscard]] const section_header* get_section(size_t n) const {
      return n >= file_header.num_sections ? nullptr : get_sections() + n;
    }

    template <typename T>
    struct section_iterator {
      T* base;
      uint16_t count;

      [[nodiscard]] T* begin() const {
        return base;
      }

      [[nodiscard]] T* end() const {
        return base + count;
      }
    };

    [[nodiscard]] section_iterator<section_header> sections() {
      return {.base = get_sections(), .count = file_header.num_sections};
    }
    [[nodiscard]] section_iterator<const section_header> sections() const {
      return {.base = get_sections(), .count = file_header.num_sections};
    }
  };

} // namespace omni::win

namespace omni::win {

  struct dos_header {
    std::uint16_t e_magic;
    std::uint16_t e_cblp;
    std::uint16_t e_cp;
    std::uint16_t e_crlc;
    std::uint16_t e_cparhdr;
    std::uint16_t e_minalloc;
    std::uint16_t e_maxalloc;
    std::uint16_t e_ss;
    std::uint16_t e_sp;
    std::uint16_t e_csum;
    std::uint16_t e_ip;
    std::uint16_t e_cs;
    std::uint16_t e_lfarlc;
    std::uint16_t e_ovno;
    std::uint16_t e_res[4];
    std::uint16_t e_oemid;
    std::uint16_t e_oeminfo;
    std::uint16_t e_res2[10];
    std::uint32_t e_lfanew;

    [[nodiscard]] file_header* get_file_header() {
      return &get_nt_headers()->file_header;
    }

    [[nodiscard]] const file_header* get_file_header() const {
      return &get_nt_headers()->file_header;
    }

    [[nodiscard]] nt_headers* get_nt_headers() {
      return reinterpret_cast<nt_headers*>(reinterpret_cast<std::byte*>(this) + e_lfanew);
    }

    [[nodiscard]] const nt_headers* get_nt_headers() const {
      return reinterpret_cast<const nt_headers*>(reinterpret_cast<const std::byte*>(this) + e_lfanew);
    }
  };

} // namespace omni::win

namespace omni::win {

  struct image {
    constexpr static auto npos{(std::numeric_limits<std::uint32_t>::max)()};

    win::dos_header dos_header;

    [[nodiscard]] win::dos_header* get_dos_headers() {
      return &dos_header;
    }

    [[nodiscard]] const win::dos_header* get_dos_headers() const {
      return &dos_header;
    }

    [[nodiscard]] file_header* get_file_header() {
      return dos_header.get_file_header();
    }

    [[nodiscard]] const file_header* get_file_header() const {
      return dos_header.get_file_header();
    }

    [[nodiscard]] nt_headers* get_nt_headers() {
      return dos_header.get_nt_headers();
    }

    [[nodiscard]] const nt_headers* get_nt_headers() const {
      return dos_header.get_nt_headers();
    }

    [[nodiscard]] optional_header* get_optional_header() {
      return &get_nt_headers()->optional_header;
    }

    [[nodiscard]] const optional_header* get_optional_header() const {
      return &get_nt_headers()->optional_header;
    }

    [[nodiscard]] data_directory* get_directory(directory_id id) {
      nt_headers* nt_hdrs = get_nt_headers();
      if (nt_hdrs->optional_header.num_data_directories <= id) {
        return nullptr;
      }
      data_directory* dir = &nt_hdrs->optional_header.data_directories.entries[id];
      return dir->present() ? dir : nullptr;
    }

    [[nodiscard]] const data_directory* get_directory(directory_id id) const {
      const nt_headers* nt_hdrs = get_nt_headers();
      if (nt_hdrs->optional_header.num_data_directories <= id) {
        return nullptr;
      }
      const data_directory* dir = &nt_hdrs->optional_header.data_directories.entries[id];
      return dir->present() ? dir : nullptr;
    }

    template <typename T = std::byte>
    [[nodiscard]] T* rva_to_ptr(std::uint32_t rva, std::size_t length = 1) {
      // Find the section, try mapping to header if none found
      section_header* scn = rva_to_section(rva);
      if (!scn) {
        std::uint32_t rva_hdr_end = get_nt_headers()->optional_header.size_headers;
        if (rva < rva_hdr_end && (rva + length) <= rva_hdr_end) {
          return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(&dos_header) + rva);
        }
        return nullptr;
      }

      // Apply the boundary check
      std::size_t offset = rva - scn->virtual_address;
      if ((offset + length) > scn->size_raw_data) {
        return nullptr;
      }

      return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(&dos_header) + scn->ptr_raw_data + offset);
    }

    template <typename T = std::byte>
    [[nodiscard]] const T* rva_to_ptr(std::uint32_t rva, std::size_t length = 1) const {
      // Find the section, try mapping to header if none found
      const section_header* scn = rva_to_section(rva);
      if (!scn) {
        std::uint32_t rva_hdr_end = get_nt_headers()->optional_header.size_headers;
        if (rva < rva_hdr_end && (rva + length) <= rva_hdr_end) {
          return reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(&dos_header) + rva);
        }
        return nullptr;
      }

      // Apply the boundary check
      std::size_t offset = rva - scn->virtual_address;
      if ((offset + length) > scn->size_raw_data) {
        return nullptr;
      }

      return reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(&dos_header) + scn->ptr_raw_data + offset);
    }

    [[nodiscard]] section_header* rva_to_section(std::uint32_t rva) {
      for (section_header& section : get_nt_headers()->sections()) {
        if (section.virtual_address <= rva && rva < (section.virtual_address + section.virtual_size)) {
          return std::addressof(section);
        }
      }
      return nullptr;
    }

    [[nodiscard]] const section_header* rva_to_section(std::uint32_t rva) const {
      for (const section_header& section : get_nt_headers()->sections()) {
        if (section.virtual_address <= rva && rva < (section.virtual_address + section.virtual_size)) {
          return std::addressof(section);
        }
      }
      return nullptr;
    }

    [[nodiscard]] std::uint32_t ptr_to_raw(const void* ptr) const {
      if (ptr == nullptr) {
        return npos;
      }

      return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(ptr) - reinterpret_cast<std::uintptr_t>(&dos_header));
    }

    [[nodiscard]] std::uint32_t rva_to_fo(std::uint32_t rva, std::size_t length = 1) const {
      return ptr_to_raw(rva_to_ptr(rva, length));
    }
  };

} // namespace omni::win

namespace omni {

  class module_exports {
    enum class iteration_kind : std::uint8_t {
      all,
      ordinal,
      named,
    };

   public:
    module_exports() = default;
    explicit module_exports(omni::address module_base) noexcept
      : module_base_(module_base), export_dir_(get_export_directory(module_base)) {}

    [[nodiscard]] std::size_t size() const noexcept {
      if (export_dir_ == nullptr) {
        return 0;
      }

      return export_dir_->num_functions;
    }

    [[nodiscard]] const win::export_directory* directory() const noexcept {
      return export_dir_;
    }

    [[nodiscard]] std::string_view name(std::size_t index) const noexcept {
      if (export_dir_ == nullptr || index >= size()) {
        return {};
      }

      const std::uint16_t* ordinal_table_ptr = export_dir_->ordinal_table(module_base_.value());
      const auto* rva_names_ptr = module_base_.offset<const std::uint32_t*>(export_dir_->rva_names);

      for (std::size_t name_index{}; name_index < export_dir_->num_names; ++name_index) {
        if (static_cast<std::size_t>(ordinal_table_ptr[name_index]) == index) {
          const auto* export_name_ptr = module_base_.offset<const char*>(rva_names_ptr[name_index]);
          return {export_name_ptr};
        }
      }

      return {};
    }

    [[nodiscard]] std::uint32_t ordinal(std::size_t index) const noexcept {
      if (export_dir_ == nullptr || index >= size()) {
        return 0;
      }

      return static_cast<std::uint32_t>(export_dir_->base + index);
    }

    [[nodiscard]] omni::address address(std::size_t index) const noexcept {
      if (export_dir_ == nullptr || index >= size()) {
        return {};
      }

      const auto* rva_table_ptr = export_dir_->rva_table(module_base_.value());
      const auto rva_function = rva_table_ptr[index];

      return module_base_.offset(rva_function);
    }

    [[nodiscard]] bool is_export_forwarded(omni::address export_address) const noexcept {
      if (export_dir_ == nullptr) {
        return false;
      }

      const auto* image = module_base_.ptr<const win::image>();
      const auto export_data_dir = image->get_optional_header()->data_directories.export_directory;

      const auto export_table_start = module_base_.offset(export_data_dir.rva);
      const auto export_table_end = export_table_start.offset(export_data_dir.size);

      return export_address >= export_table_start && export_address < export_table_end;
    }

    template <iteration_kind IterationKind>
    class export_iterator {
     public:
      using iterator_category = std::bidirectional_iterator_tag;
      using value_type = module_export;
      using difference_type = std::ptrdiff_t;
      using pointer = const value_type*;
      using reference = const value_type&;

      export_iterator() noexcept: module_exports_(nullptr), index_(0), current_export_() {}
      ~export_iterator() = default;
      export_iterator(const export_iterator&) = default;
      export_iterator(export_iterator&&) = default;
      export_iterator& operator=(export_iterator&&) = default;

      export_iterator(const module_exports* exports, std::size_t index) noexcept
        : module_exports_(exports), index_(index), current_export_() {
        update_current_export();
      }

      [[nodiscard]] reference operator*() const noexcept {
        return current_export_;
      }

      [[nodiscard]] pointer operator->() const noexcept {
        return &current_export_;
      }

      export_iterator& operator=(const export_iterator& other) noexcept {
        if (this != &other) {
          module_exports_ = other.module_exports_;
          index_ = other.index_;
          current_export_ = other.current_export_;
        }

        return *this;
      }

      export_iterator& operator++() noexcept {
        if (module_exports_ == nullptr) {
          return *this;
        }

        if (index_ < module_exports_->iteration_size<IterationKind>()) {
          ++index_;
        }

        update_current_export();
        return *this;
      }

      export_iterator operator++(int) noexcept {
        export_iterator temp = *this;
        ++(*this);
        return temp;
      }

      export_iterator& operator--() noexcept {
        if (module_exports_ == nullptr) {
          return *this;
        }

        if (index_ > 0) {
          --index_;
        }

        update_current_export();
        return *this;
      }

      export_iterator operator--(int) noexcept {
        export_iterator temp = *this;
        --(*this);
        return temp;
      }

      [[nodiscard]] bool operator==(const export_iterator& other) const noexcept {
        return index_ == other.index_ && module_exports_ == other.module_exports_;
      }

      [[nodiscard]] bool operator!=(const export_iterator& other) const noexcept {
        return !(*this == other);
      }

     private:
      void update_current_export() noexcept {
        if (module_exports_ == nullptr || index_ >= module_exports_->iteration_size<IterationKind>()) {
          current_export_ = value_type{};
          return;
        }

        // Fast path for named exports to avoid O(n) search
        if constexpr (IterationKind == iteration_kind::named) {
          const auto* ordinal_table_ptr = module_exports_->export_dir_->ordinal_table(module_exports_->module_base_.value());
          const auto* rva_names_ptr =
            module_exports_->module_base_.offset<const std::uint32_t*>(module_exports_->export_dir_->rva_names);

          const auto function_index = static_cast<std::size_t>(ordinal_table_ptr[index_]);
          const auto* export_name_ptr = module_exports_->module_base_.offset<const char*>(rva_names_ptr[index_]);
          const auto export_address = module_exports_->address(function_index);
          bool is_export_forwarded = module_exports_->is_export_forwarded(export_address);

          current_export_ = value_type{
            .name = std::string_view{export_name_ptr},
            .address = export_address,
            .ordinal = module_exports_->ordinal(function_index),
            .is_forwarded = is_export_forwarded,
            .module_base = module_exports_->module_base_,
          };

          if (is_export_forwarded) {
            current_export_.forwarder_string = forwarder_string::parse(export_address.ptr<const char>());
          }

          return;
        }

        const auto function_index = index_;
        const auto export_address = module_exports_->address(function_index);
        bool is_export_forwarded = module_exports_->is_export_forwarded(export_address);

        current_export_ = value_type{
          .name = get_export_name(function_index),
          .address = export_address,
          .ordinal = module_exports_->ordinal(function_index),
          .is_forwarded = is_export_forwarded,
          .module_base = module_exports_->module_base_,
        };

        if (is_export_forwarded) {
          current_export_.forwarder_string = forwarder_string::parse(export_address.ptr<const char>());
        }
      }

      [[nodiscard]] std::string_view get_export_name(std::size_t function_index) const noexcept {
        // Search for export name, which is O(n) only in case of
        // iterating over a range that includes named exports
        if constexpr (IterationKind == iteration_kind::all || IterationKind == iteration_kind::named) {
          return module_exports_->name(function_index);
        }
        return {};
      }

      const module_exports* module_exports_;
      std::size_t index_;
      mutable value_type current_export_;
    };

    template <iteration_kind IterationKind>
    class export_range {
     public:
      export_range() noexcept: module_exports_(nullptr) {}
      explicit export_range(const module_exports* exports) noexcept: module_exports_(exports) {}

      [[nodiscard]] export_iterator<IterationKind> begin() const noexcept {
        return {module_exports_, 0};
      }

      [[nodiscard]] export_iterator<IterationKind> end() const noexcept {
        if (module_exports_ == nullptr) {
          return {nullptr, 0};
        }

        return {module_exports_, module_exports_->iteration_size<IterationKind>()};
      }

     private:
      const module_exports* module_exports_;
    };

    using iterator = export_iterator<iteration_kind::all>;
    using ordinal_iterator = export_iterator<iteration_kind::ordinal>;
    using named_iterator = export_iterator<iteration_kind::named>;

    using all_range = export_range<iteration_kind::all>;
    using ordinal_range = export_range<iteration_kind::ordinal>;
    using named_range = export_range<iteration_kind::named>;

    static_assert(std::bidirectional_iterator<iterator>);
    static_assert(std::bidirectional_iterator<ordinal_iterator>);
    static_assert(std::bidirectional_iterator<named_iterator>);

    [[nodiscard]] iterator begin() const& noexcept {
      return {this, 0};
    }

    [[nodiscard]] iterator end() const& noexcept {
      return {this, iteration_size<iteration_kind::all>()};
    }

    [[nodiscard]] all_range all() const& noexcept {
      return all_range{this};
    }

    [[nodiscard]] ordinal_range ordinal() const& noexcept {
      return ordinal_range{this};
    }

    [[nodiscard]] named_range named() const& noexcept {
      return named_range{this};
    }

    [[nodiscard]] iterator find(concepts::hash auto export_name) const& noexcept {
      return find_by_name_hash(export_name);
    }

    [[nodiscard]] iterator find(default_hash export_name) const& noexcept {
      return find_by_name_hash(export_name);
    }

    [[nodiscard]] iterator find(std::uint32_t ordinal_value, omni::use_ordinal_t) const& noexcept {
      if (export_dir_ == nullptr || ordinal_value < export_dir_->base) {
        return end();
      }

      const auto function_index = static_cast<std::size_t>(ordinal_value - export_dir_->base);
      if (function_index >= size()) {
        return end();
      }

      return {this, function_index};
    }

    [[nodiscard]] iterator find_if(std::predicate<const typename iterator::value_type&> auto predicate) const& {
      if (export_dir_ == nullptr) {
        return end();
      }

      return std::ranges::find_if(*this, predicate);
    }

    // NOLINTBEGIN(modernize-use-nodiscard)

    iterator begin() const&& = delete;
    iterator end() const&& = delete;
    all_range all() const&& = delete;
    ordinal_range ordinal() const&& = delete;
    named_range named() const&& = delete;
    iterator find(concepts::hash auto export_name) const&& = delete;
    iterator find(default_hash export_name) const&& = delete;
    iterator find(std::uint32_t ordinal_value, omni::use_ordinal_t) const&& = delete;
    iterator find_if(std::predicate<const typename iterator::value_type&> auto predicate) const&& = delete;

    // NOLINTEND(modernize-use-nodiscard)

   private:
    template <iteration_kind IterationKind>
    [[nodiscard]] std::size_t iteration_size() const noexcept {
      if (export_dir_ == nullptr) {
        return 0;
      }

      if constexpr (IterationKind == iteration_kind::named) {
        return export_dir_->num_names;
      }

      return export_dir_->num_functions;
    }

    template <typename Hash>
    [[nodiscard]] iterator find_by_name_hash(Hash export_name) const noexcept {
      if (export_dir_ == nullptr) {
        return end();
      }

      const auto* ordinal_table_ptr = export_dir_->ordinal_table(module_base_.value());
      const auto* rva_names_ptr = module_base_.offset<const std::uint32_t*>(export_dir_->rva_names);

      for (std::size_t name_index{}; name_index < export_dir_->num_names; ++name_index) {
        const auto* export_name_ptr = module_base_.offset<const char*>(rva_names_ptr[name_index]);
        const auto export_name_view = std::string_view{export_name_ptr};

        if (!export_name_view.empty() && omni::hash<Hash>(export_name_view) == export_name) {
          return {this, static_cast<std::size_t>(ordinal_table_ptr[name_index])};
        }
      }

      return end();
    }

    static win::export_directory* get_export_directory(omni::address base_address) {
      if (!base_address) {
        return nullptr;
      }

      const auto* image = base_address.ptr<const win::image>();
      const auto export_data_dir = image->get_optional_header()->data_directories.export_directory;
      if (!export_data_dir.present()) {
        return nullptr;
      }

      return base_address.ptr<win::export_directory>(export_data_dir.rva);
    }

    omni::address module_base_;
    win::export_directory* export_dir_{nullptr};
  };

  static_assert(std::ranges::viewable_range<module_exports>);
  static_assert(std::ranges::viewable_range<module_exports::all_range>);
  static_assert(std::ranges::viewable_range<module_exports::ordinal_range>);
  static_assert(std::ranges::viewable_range<module_exports::named_range>);

} // namespace omni

#include <cstdint>

#if defined(OMNI_COMPILER_MSVC_COMPAT)
#  include <intrin.h>
#endif

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

namespace omni::win {

  struct list_entry {
    list_entry* forward_link;
    list_entry* backward_link;
  };

  struct user_process_parameters {
    std::uint8_t reserved1[16];
    void* reserved2[10];
    unicode_string image_path_name;
    unicode_string command_line;
  };

  struct loader_table_entry {
    list_entry in_load_order_links;
    list_entry in_memory_order_links;
    union {
      list_entry in_initialization_order_links;
      list_entry in_progress_links;
    };
    omni::address base_address;
    omni::address entry_point;
    std::uint32_t size_image;
    unicode_string path;
    unicode_string name;
    union {
      std::uint8_t flag_group[4];
      std::uint32_t flags;
      struct {
        std::uint32_t packaged_binary:1;
        std::uint32_t marked_for_removal:1;
        std::uint32_t image_module:1;
        std::uint32_t load_notifications_sent:1;
        std::uint32_t telemetry_entry_processed:1;
        std::uint32_t static_import_processed:1;
        std::uint32_t in_legacy_lists:1;
        std::uint32_t in_indexes:1;
        std::uint32_t shim_module:1;
        std::uint32_t in_exception_table:1;
        std::uint32_t reserved_flags_1:2;
        std::uint32_t load_in_progress:1;
        std::uint32_t load_config_processed:1;
        std::uint32_t entry_point_processed:1;
        std::uint32_t delay_load_protection_enabled:1;
        std::uint32_t reserved_flags_3:2;
        std::uint32_t skip_thread_calls:1;
        std::uint32_t process_attach_called:1;
        std::uint32_t process_attach_failed:1;
        std::uint32_t cor_validation_deferred:1;
        std::uint32_t is_cor_image:1;
        std::uint32_t skip_relocation:1;
        std::uint32_t is_cor_il_only:1;
        std::uint32_t is_chpe_image:1;
        std::uint32_t reserved_flags_5:2;
        std::uint32_t redirected:1;
        std::uint32_t reserved_flags_6:2;
        std::uint32_t compatibility_database_processed:1;
      };
    };
    std::uint16_t obsolete_load_count;
    std::uint16_t tls_index;
    list_entry hash_links;
    std::uint32_t time_date_stamp;
  };

  struct module_loader_data {
    std::uint32_t length;
    std::uint8_t initialized;
    void* ss_handle;
    list_entry in_load_order_module_list;
    list_entry in_memory_order_module_list;
    list_entry in_initialization_order_module_list;
  };

  struct PEB {
    std::uint8_t reserved1[2];
    std::uint8_t being_debugged;
    std::uint8_t reserved2[1];
    void* reserved3[2];
    module_loader_data* loader_data;
    user_process_parameters* process_parameters;
    void* reserved4[3];
    void* atl_thunk_list_head;
    void* reserved5;
    std::uint32_t reserved6;
    void* reserved7;
    std::uint32_t reserved8;
    std::uint32_t atl_thunk_list_head32;
    win::api_set_namespace* api_set_map;
    void* reserved9[44];
    std::uint8_t reserved10[96];

    [[nodiscard]] static auto ptr() {
#if defined(OMNI_ARCH_X64)
#  if defined(OMNI_COMPILER_MSVC_COMPAT)
      return reinterpret_cast<const PEB*>(__readgsqword(0x60));
#  else
      std::uintptr_t address{};
      __asm__ __volatile__("movq %%gs:0x60, %0" : "=r"(address));
      return reinterpret_cast<const PEB*>(address);
#  endif
#elif defined(OMNI_ARCH_X86)
#  if defined(OMNI_COMPILER_MSVC_COMPAT)
      return reinterpret_cast<const PEB*>(__readfsdword(0x30));
#  else
      std::uintptr_t address{};
      __asm__ __volatile__("movl %%fs:0x30, %0" : "=r"(address));
      return reinterpret_cast<const PEB*>(address);
#  endif
#else
#  error Unsupported platform.
#endif
    }
  };

  template <typename T, typename FieldT>
  [[nodiscard]] constexpr T* export_containing_record(FieldT* address, FieldT T::* field) {
    auto offset = reinterpret_cast<std::uintptr_t>(&(reinterpret_cast<T*>(0)->*field));
    return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(address) - offset);
  }

} // namespace omni::win

namespace omni {

  class module {
   public:
    module() = default;
    explicit module(win::loader_table_entry* module_data): entry_(module_data) {}

    [[nodiscard]] win::loader_table_entry* entry() noexcept {
      return entry_;
    }

    [[nodiscard]] const win::loader_table_entry* loader_entry() const noexcept {
      return entry_;
    }

    [[nodiscard]] win::image* image() noexcept {
      return assert_entry()->base_address.ptr<win::image>();
    }

    [[nodiscard]] const win::image* image() const noexcept {
      return assert_entry()->base_address.ptr<win::image>();
    }

    [[nodiscard]] omni::address base_address() const noexcept {
      return assert_entry()->base_address;
    }

    [[nodiscard]] void* native_handle() const noexcept {
      return assert_entry()->base_address.ptr();
    }

    [[nodiscard]] omni::address entry_point() const noexcept {
      return assert_entry()->entry_point;
    }

    [[nodiscard]] std::string name() const {
      return assert_entry()->name.string();
    }

    [[nodiscard]] std::wstring_view wname() const noexcept {
      return static_cast<std::wstring_view>(assert_entry()->name);
    }

    [[nodiscard]] std::filesystem::path system_path(
      std::filesystem::path::format fmt = std::filesystem::path::auto_format) const {
      return assert_entry()->path.to_path(fmt);
    }

    [[nodiscard]] module_exports exports() const noexcept {
      return module_exports{assert_entry()->base_address};
    }

    [[nodiscard]] bool present() const noexcept {
      return entry_ != nullptr;
    }

    [[nodiscard]] bool operator==(const module& other) const noexcept {
      return entry_ == other.entry_;
    }

    [[nodiscard]] bool matches_name_hash(concepts::hash auto module_name_hash) const noexcept {
      return compare_hashed_module_name(module_name_hash, wname());
    }

    [[nodiscard]] bool matches_name_hash(default_hash module_name_hash) const noexcept {
      return compare_hashed_module_name(module_name_hash, wname());
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return present();
    }

    friend std::ostream& operator<<(std::ostream& os, const module& module) {
      return os << module.name();
    }

   private:
    template <concepts::hash Hasher>
    static bool compare_hashed_module_name(Hasher module_name_hash, std::wstring_view module_name) {
      constexpr Hasher dll_suffix_hash{L".dll"};

      // Fast path most of the time, hash and compare full names
      if (module_name_hash == omni::hash<Hasher>(module_name)) {
        return true;
      }

      // When name is shorter than ".dll" means that suffix is already stripped
      auto name_length = module_name.length();
      if (name_length <= 4) {
        return false;
      }

      // Skip modules with suffixes other than ".dll" and pure names that didn't match
      auto module_suffix_hash = omni::hash<Hasher>(module_name.substr(name_length - 4));
      if (module_suffix_hash != dll_suffix_hash) {
        return false;
      }

      // Trim ".dll" suffix and compare pure module names
      auto trimmed_name_hash = omni::hash<Hasher>(module_name.substr(0, name_length - 4));
      return module_name_hash == trimmed_name_hash;
    }

    [[nodiscard]] win::loader_table_entry* assert_entry() noexcept {
      assert(entry_ != nullptr);
      return entry_;
    }

    [[nodiscard]] const win::loader_table_entry* assert_entry() const noexcept {
      assert(entry_ != nullptr);
      return entry_;
    }

    win::loader_table_entry* entry_{nullptr};
  };

} // namespace omni

template <>
struct std::formatter<omni::module> : std::formatter<std::string_view> {
  auto format(const omni::module& module, std::format_context& ctx) const {
    std::string module_name;
    if (module.present()) {
      module_name = module.name();
    }
    return std::formatter<std::string_view>{}.format(module_name, ctx);
  }
};

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

  inline module_export get_export(concepts::hash auto export_name, omni::module module);
  inline module_export get_export(default_hash export_name, omni::module module);

  template <concepts::hash Hasher>
  inline module_export get_export(Hasher export_name, Hasher module_name);
  inline module_export get_export(default_hash export_name, default_hash module_name);

  inline module_export get_export(concepts::hash auto export_name);
  inline module_export get_export(default_hash export_name);

  template <concepts::hash Hasher>
  inline module_export get_export(std::uint32_t ordinal, omni::module module, omni::use_ordinal_t);
  inline module_export get_export(std::uint32_t ordinal, omni::module module, omni::use_ordinal_t);
  inline module_export get_export(std::uint32_t ordinal, concepts::hash auto module_name, omni::use_ordinal_t);
  inline module_export get_export(std::uint32_t ordinal, default_hash module_name, omni::use_ordinal_t);

} // namespace omni

#include <cstdint>
#include <ranges>
#include <span>
#include <string_view>

namespace omni {

  namespace detail {

    [[nodiscard]] constexpr bool is_decimal(std::wstring_view value) noexcept {
      return !value.empty() && std::ranges::all_of(value, [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; });
    }

    [[nodiscard]] constexpr std::size_t find_api_set_version_suffix(std::wstring_view contract_name) noexcept {
      const std::size_t patch_dash_pos = contract_name.rfind(L'-');
      if (patch_dash_pos == std::wstring_view::npos || patch_dash_pos == 0 ||
          !is_decimal(contract_name.substr(patch_dash_pos + 1))) {
        return std::wstring_view::npos;
      }

      const std::size_t minor_dash_pos = contract_name.rfind(L'-', patch_dash_pos - 1);
      if (minor_dash_pos == std::wstring_view::npos || minor_dash_pos == 0 ||
          !is_decimal(contract_name.substr(minor_dash_pos + 1, patch_dash_pos - minor_dash_pos - 1))) {
        return std::wstring_view::npos;
      }

      const std::size_t level_dash_pos = contract_name.rfind(L'-', minor_dash_pos - 1);
      if (level_dash_pos == std::wstring_view::npos) {
        return std::wstring_view::npos;
      }

      const auto level = contract_name.substr(level_dash_pos + 1, minor_dash_pos - level_dash_pos - 1);
      if (level.size() < 2 || level.front() != L'l' || !is_decimal(level.substr(1))) {
        return std::wstring_view::npos;
      }

      return level_dash_pos;
    }

    [[nodiscard]] constexpr std::wstring_view remove_api_set_version(std::wstring_view contract_name) noexcept {
      auto version_pos = find_api_set_version_suffix(contract_name);
      if (version_pos == std::wstring_view::npos) {
        return {};
      }

      return contract_name.substr(0, version_pos);
    }

    [[nodiscard]] constexpr std::wstring_view remove_dll_suffix(std::wstring_view contract_name) noexcept {
      constexpr std::size_t dll_suffix_length = 4; // ".dll"

      if (contract_name.size() < dll_suffix_length) {
        return contract_name;
      }

      auto suffix = contract_name.substr(contract_name.size() - dll_suffix_length);
      if (omni::hash<omni::default_hash>(suffix) != omni::default_hash{L".dll"}) {
        return contract_name;
      }

      return contract_name.substr(0, contract_name.size() - dll_suffix_length);
    }

  } // namespace detail

  class api_sets {
   public:
    api_sets(): api_set_map_(win::PEB::ptr()->api_set_map) {}

    class iterator {
     public:
      using iterator_category = std::bidirectional_iterator_tag;
      using value_type = api_set;
      using difference_type = std::ptrdiff_t;
      using pointer = const value_type*;
      using reference = const value_type&;

      iterator() noexcept: api_set_map_(nullptr), index_(0) {}

      iterator(win::api_set_namespace* api_set_map, std::uint32_t index): api_set_map_(api_set_map), index_(index) {
        update_value();
      }

      [[nodiscard]] reference operator*() const noexcept {
        return current_;
      }

      [[nodiscard]] pointer operator->() const noexcept {
        return &current_;
      }

      iterator& operator++() noexcept {
        if (api_set_map_ == nullptr) {
          return *this;
        }

        if (index_ < api_set_map_->entries_count) {
          ++index_;
          update_value();
        }
        return *this;
      }

      iterator operator++(int) noexcept {
        iterator tmp = *this;
        ++(*this);
        return tmp;
      }

      iterator& operator--() noexcept {
        if (api_set_map_ == nullptr) {
          return *this;
        }

        if (index_ > 0) {
          --index_;
          update_value();
        }
        return *this;
      }

      iterator operator--(int) noexcept {
        iterator tmp = *this;
        --(*this);
        return tmp;
      }

      bool operator==(const iterator& other) const noexcept {
        return api_set_map_ == other.api_set_map_ && index_ == other.index_;
      }

      bool operator!=(const iterator& other) const noexcept {
        return !(*this == other);
      }

     private:
      void update_value() {
        if (api_set_map_ == nullptr || index_ >= api_set_map_->entries_count) {
          current_ = value_type{};
          return;
        }

        omni::address api_set_map_address{api_set_map_};

        const win::api_set_namespace_entry* namespace_entry = api_set_map_->get_namespace_entry(index_);
        std::span value_entries = namespace_entry->value_entries(api_set_map_address);

        bool is_entry_sealed = namespace_entry->is_sealed();
        std::wstring_view contract_name = namespace_entry->contract_name(api_set_map_address);

        current_ = value_type{contract_name, is_entry_sealed, value_entries, omni::address{api_set_map_}};
      }

      win::api_set_namespace* api_set_map_;
      std::uint32_t index_;
      mutable value_type current_;
    };

    static_assert(std::bidirectional_iterator<iterator>);

    [[nodiscard]] iterator begin() const noexcept {
      return {api_set_map_, 0};
    }

    [[nodiscard]] iterator end() const noexcept {
      return {api_set_map_, static_cast<std::uint32_t>(size())};
    }

    [[nodiscard]] std::size_t size() const noexcept {
      if (api_set_map_ == nullptr) {
        return 0;
      }

      return api_set_map_->entries_count;
    }

    [[nodiscard]] iterator find(concepts::hash auto contract_name_hash) const noexcept {
      using hash_type = decltype(contract_name_hash);

      for (auto it = begin(); it != end(); ++it) {
        auto canonical_contract_name = detail::remove_dll_suffix(it->contract_name());
        if (contract_name_hash == omni::hash<hash_type>(canonical_contract_name)) {
          return it;
        }

        auto versionless_contract_name = detail::remove_api_set_version(canonical_contract_name);
        if (versionless_contract_name.empty()) {
          continue;
        }

        if (contract_name_hash == omni::hash<hash_type>(versionless_contract_name)) {
          return it;
        }
      }

      return end();
    }

    [[nodiscard]] iterator find(default_hash contract_name) const noexcept {
      return find<default_hash>(contract_name);
    }

    [[nodiscard]] iterator find_if(std::predicate<iterator::value_type> auto pred) const {
      for (auto it = begin(); it != end(); ++it) {
        if (pred(*it)) {
          return it;
        }
      }
      return end();
    }

   private:
    win::api_set_namespace* api_set_map_{};
  };

  static_assert(std::ranges::viewable_range<api_sets>);

  inline omni::api_set get_api_set(concepts::hash auto contract_name) noexcept {
    omni::api_sets api_sets;
    auto it = api_sets.find(contract_name);
    if (it == api_sets.end()) {
      return {};
    }
    return *it;
  }

  inline omni::api_set get_api_set(default_hash contract_name) noexcept {
    return get_api_set<default_hash>(contract_name);
  }

} // namespace omni

namespace omni {

  namespace detail {

    template <concepts::hash Hasher>
    inline module_export resolve_forwarded_export(omni::address export_address) {
      // Learn more here: https://devblogs.microsoft.com/oldnewthing/20060719-24/?p=30473
      // In a forwarded export, the address is a string containing
      // information about the actual export and its location
      // They are always presented as "module_name.export_name"
      const auto* forwarder_str = export_address.ptr<const char>();

      // Split forwarded export to module name and real export name
      forwarder_string forwarder{forwarder_string::parse(forwarder_str)};
      if (forwarder.function.empty()) {
        return {};
      }

      const auto resolve_export_in_module = [&forwarder](Hasher module_name_hash) -> module_export {
        if (forwarder.is_ordinal()) {
          return omni::get_export<Hasher>(forwarder.to_ordinal(), module_name_hash, omni::use_ordinal);
        }
        return omni::get_export<Hasher>(omni::hash<Hasher>(forwarder.function), module_name_hash);
      };

      // Try to search for the real export with a pre-known module name
      module_export real_export = resolve_export_in_module(omni::hash<Hasher>(forwarder.module));
      if (real_export.present()) {
        return real_export;
      }

      // Direct lookup by forwarder target failed, try api-set resolution next
      module_export forwarded_export{
        .is_forwarded = true,
        .forwarder_string = forwarder,
      };

      // We cannot know whether a module is an API-set. The only way
      // to find out is to scan the names of API-set contracts to
      // see if its name is among them
      auto api_set = omni::get_api_set(omni::hash<Hasher>(forwarder.module));
      if (!api_set.present()) {
        return forwarded_export;
      }

      // At this stage, we know that the module pointed to by the forwarder
      // is an API set. We will iterate through its list of hosts to find
      // any loaded module, in which we will attempt to locate the actual
      // export name given by forwarder
      for (const omni::api_set_host& host : api_set.hosts()) {
        bool host_name_empty = not host.present() or host.value.empty();
        if (host_name_empty) {
          continue;
        }

        auto module_name_hash = omni::hash<Hasher>(host.value);
        module_export host_export = resolve_export_in_module(module_name_hash);
        if (host_export.present()) {
          return host_export;
        }
      }

      // The API-set host specified by the forwarder has not been loaded
      // into the process. We've done everything we can, so we return
      // the API-set that we couldn't resolve to the caller
      forwarded_export.forwarder_api_set = api_set;

      return forwarded_export;
    }

  } // namespace detail

  inline omni::module base_module() {
    return *omni::modules{}.begin();
  }

  inline omni::module get_module(concepts::hash auto module_name) {
    omni::modules modules{};
    auto it = modules.find(module_name);
    if (it == modules.end()) {
      return {};
    }

    return *it;
  }

  inline omni::module get_module(default_hash module_name) {
    return get_module<default_hash>(module_name);
  }

  inline omni::module get_module(omni::address base_address) {
    omni::modules modules{};
    auto it = modules.find(base_address);
    if (it == modules.end()) {
      return {};
    }

    return *it;
  }

  inline module_export get_export(concepts::hash auto export_name, omni::module module) {
    if (!module.present()) {
      return {};
    }

    auto exports = module.exports();
    auto export_it = exports.find(export_name);
    if (export_it == exports.end()) {
      // The named export that the caller is looking for was
      // not found in the expected module
      return {};
    }

    if (export_it->is_forwarded) {
      return detail::resolve_forwarded_export<decltype(export_name)>(export_it->address);
    }

    return *export_it;
  }

  inline module_export get_export(default_hash export_name, omni::module module) {
    return omni::get_export<default_hash>(export_name, module);
  }

  template <concepts::hash Hasher>
  inline module_export get_export(Hasher export_name, Hasher module_name) {
    return omni::get_export<Hasher>(export_name, omni::get_module(module_name));
  }

  inline module_export get_export(default_hash export_name, default_hash module_name) {
    return omni::get_export<default_hash>(export_name, omni::get_module(module_name));
  }

  inline module_export get_export(concepts::hash auto export_name) {
    for (const omni::module& module : omni::modules{}) {
      if (auto module_export = omni::get_export(export_name, module); module_export) {
        return module_export;
      }
    }
    return {};
  }

  inline module_export get_export(default_hash export_name) {
    return omni::get_export<default_hash>(export_name);
  }

  template <concepts::hash Hasher>
  inline module_export get_export(std::uint32_t ordinal, omni::module module, omni::use_ordinal_t) {
    if (!module.present()) {
      return {};
    }

    auto exports = module.exports();
    auto export_it = exports.find(ordinal, omni::use_ordinal);
    if (export_it == exports.end()) {
      // The ordinal export that the caller is looking for was
      // not found in the expected module
      return {};
    }

    if (export_it->is_forwarded) {
      return detail::resolve_forwarded_export<Hasher>(export_it->address);
    }

    return *export_it;
  }

  inline module_export get_export(std::uint32_t ordinal, omni::module module, omni::use_ordinal_t) {
    return omni::get_export<default_hash>(ordinal, module, omni::use_ordinal);
  }

  inline module_export get_export(std::uint32_t ordinal, concepts::hash auto module_name, omni::use_ordinal_t) {
    return omni::get_export<decltype(module_name)>(ordinal, omni::get_module(module_name), omni::use_ordinal);
  }

  inline module_export get_export(std::uint32_t ordinal, default_hash module_name, omni::use_ordinal_t) {
    return omni::get_export<default_hash>(ordinal, module_name, omni::use_ordinal);
  }

} // namespace omni

#include <compare>
#include <cstdint>
#include <format>
#include <type_traits>
#include <utility>

namespace omni {

  enum class severity : std::uint8_t {
    success = 0,
    information = 1,
    warning = 2,
    error = 3,
  };

  enum class facility : std::uint16_t {
    debugger = 0x1,
    rpc_runtime = 0x2,
    rpc_stubs = 0x3,
    io_error_code = 0x4,
    codclass_error_code = 0x6,
    ntwin32 = 0x7,
    ntcert = 0x8,
    ntsspi = 0x9,
    terminal_server = 0xA,
    mui_error_code = 0xB,
    usb_error_code = 0x10,
    hid_error_code = 0x11,
    firewire_error_code = 0x12,
    cluster_error_code = 0x13,
    acpi_error_code = 0x14,
    sxs_error_code = 0x15,
    transaction = 0x19,
    commonlog = 0x1A,
    video = 0x1B,
    filter_manager = 0x1C,
    monitor = 0x1D,
    graphics_kernel = 0x1E,
    driver_framework = 0x20,
    fve_error_code = 0x21,
    fwp_error_code = 0x22,
    ndis_error_code = 0x23,
    tpm = 0x29,
    rtpm = 0x2A,
    hypervisor = 0x35,
    ipsec = 0x36,
    virtualization = 0x37,
    volmgr = 0x38,
    bcd_error_code = 0x39,
    win32k_ntuser = 0x3E,
    win32k_ntgdi = 0x3F,
    resume_key_filter = 0x40,
    rdbss = 0x41,
    bth_att = 0x42,
    secureboot = 0x43,
    audio_kernel = 0x44,
    vsm = 0x45,
    volsnap = 0x50,
    sdbus = 0x51,
    shared_vhdx = 0x5C,
    smb = 0x5D,
    interix = 0x99,
    spaces = 0xE7,
    security_core = 0xE8,
    system_integrity = 0xE9,
    licensing = 0xEA,
    platform_manifest = 0xEB,
    app_exec = 0xEC,
    maximum_value = 0xED,
  };

  enum class ntstatus : std::int32_t {
    success = static_cast<std::int32_t>(0x00000000),
    pending = static_cast<std::int32_t>(0x00000103),
    timeout = static_cast<std::int32_t>(0x00000102),
    more_entries = static_cast<std::int32_t>(0x00000105),
    no_more_entries = static_cast<std::int32_t>(0x8000001A),
    no_more_files = static_cast<std::int32_t>(0x80000006),
    buffer_overflow = static_cast<std::int32_t>(0x80000005),

    unsuccessful = static_cast<std::int32_t>(0xC0000001),
    not_implemented = static_cast<std::int32_t>(0xC0000002),
    not_supported = static_cast<std::int32_t>(0xC00000BB),
    invalid_info_class = static_cast<std::int32_t>(0xC0000003),
    info_length_mismatch = static_cast<std::int32_t>(0xC0000004),
    invalid_handle = static_cast<std::int32_t>(0xC0000008),
    invalid_parameter = static_cast<std::int32_t>(0xC000000D),
    invalid_parameter_mix = static_cast<std::int32_t>(0xC0000030),
    invalid_parameter_1 = static_cast<std::int32_t>(0xC00000EF),
    invalid_parameter_2 = static_cast<std::int32_t>(0xC00000F0),
    invalid_parameter_3 = static_cast<std::int32_t>(0xC00000F1),
    invalid_parameter_4 = static_cast<std::int32_t>(0xC00000F2),
    invalid_parameter_5 = static_cast<std::int32_t>(0xC00000F3),
    invalid_parameter_6 = static_cast<std::int32_t>(0xC00000F4),
    invalid_parameter_7 = static_cast<std::int32_t>(0xC00000F5),
    invalid_parameter_8 = static_cast<std::int32_t>(0xC00000F6),
    invalid_parameter_9 = static_cast<std::int32_t>(0xC00000F7),
    invalid_parameter_10 = static_cast<std::int32_t>(0xC00000F8),
    invalid_parameter_11 = static_cast<std::int32_t>(0xC00000F9),
    invalid_parameter_12 = static_cast<std::int32_t>(0xC00000FA),
    access_denied = static_cast<std::int32_t>(0xC0000022),
    object_type_mismatch = static_cast<std::int32_t>(0xC0000024),
    invalid_device_request = static_cast<std::int32_t>(0xC0000010),
    illegal_instruction = static_cast<std::int32_t>(0xC000001D),
    noncontinuable_exception = static_cast<std::int32_t>(0xC0000025),
    invalid_disposition = static_cast<std::int32_t>(0xC0000026),
    access_violation = static_cast<std::int32_t>(0xC0000005),
    in_page_error = static_cast<std::int32_t>(0xC0000006),
    buffer_too_small = static_cast<std::int32_t>(0xC0000023),

    no_memory = static_cast<std::int32_t>(0xC0000017),
    conflicting_addresses = static_cast<std::int32_t>(0xC0000018),
    not_mapped_view = static_cast<std::int32_t>(0xC0000019),
    unable_to_free_vm = static_cast<std::int32_t>(0xC000001A),
    unable_to_delete_section = static_cast<std::int32_t>(0xC000001B),
    invalid_view_size = static_cast<std::int32_t>(0xC000001F),
    invalid_file_for_section = static_cast<std::int32_t>(0xC0000020),
    already_committed = static_cast<std::int32_t>(0xC0000021),
    unable_to_decommit_vm = static_cast<std::int32_t>(0xC000002C),
    not_committed = static_cast<std::int32_t>(0xC000002D),
    invalid_page_protection = static_cast<std::int32_t>(0xC0000045),
    memory_not_allocated = static_cast<std::int32_t>(0xC00000A0),

    no_such_device = static_cast<std::int32_t>(0xC000000E),
    no_such_file = static_cast<std::int32_t>(0xC000000F),
    end_of_file = static_cast<std::int32_t>(0xC0000011),
    wrong_volume = static_cast<std::int32_t>(0xC0000012),
    no_media_in_device = static_cast<std::int32_t>(0xC0000013),
    unrecognized_media = static_cast<std::int32_t>(0xC0000014),
    nonexistent_sector = static_cast<std::int32_t>(0xC0000015),
    object_name_invalid = static_cast<std::int32_t>(0xC0000033),
    object_name_not_found = static_cast<std::int32_t>(0xC0000034),
    object_name_collision = static_cast<std::int32_t>(0xC0000035),
    object_path_invalid = static_cast<std::int32_t>(0xC0000039),
    object_path_not_found = static_cast<std::int32_t>(0xC000003A),
    object_path_syntax_bad = static_cast<std::int32_t>(0xC000003B),
    sharing_violation = static_cast<std::int32_t>(0xC0000043),
    delete_pending = static_cast<std::int32_t>(0xC0000056),
    file_is_a_directory = static_cast<std::int32_t>(0xC00000BA),
    file_renamed = static_cast<std::int32_t>(0xC00000D5),
    disk_full = static_cast<std::int32_t>(0xC000007F),
    crc_error = static_cast<std::int32_t>(0xC000003F),
    media_write_protected = static_cast<std::int32_t>(0xC00000A2),

    procedure_not_found = static_cast<std::int32_t>(0xC000007A),
    invalid_image_format = static_cast<std::int32_t>(0xC000007B),
    dll_not_found = static_cast<std::int32_t>(0xC0000135),
    ordinal_not_found = static_cast<std::int32_t>(0xC0000138),
    entrypoint_not_found = static_cast<std::int32_t>(0xC0000139),
    image_not_at_base = static_cast<std::int32_t>(0x40000003),
    object_name_exists = static_cast<std::int32_t>(0x40000000),

    thread_is_terminating = static_cast<std::int32_t>(0xC000004B),
    suspend_count_exceeded = static_cast<std::int32_t>(0xC000004A),
    process_not_in_job = static_cast<std::int32_t>(0x00000123),
    process_in_job = static_cast<std::int32_t>(0x00000124),

    invalid_owner = static_cast<std::int32_t>(0xC000005A),
    invalid_primary_group = static_cast<std::int32_t>(0xC000005B),
    no_impersonation_token = static_cast<std::int32_t>(0xC000005C),
    cant_disable_mandatory = static_cast<std::int32_t>(0xC000005D),
    no_logon_servers = static_cast<std::int32_t>(0xC000005E),
    no_such_logon_session = static_cast<std::int32_t>(0xC000005F),
    no_such_privilege = static_cast<std::int32_t>(0xC0000060),
    privilege_not_held = static_cast<std::int32_t>(0xC0000061),
    invalid_account_name = static_cast<std::int32_t>(0xC0000062),
    user_exists = static_cast<std::int32_t>(0xC0000063),
    no_such_user = static_cast<std::int32_t>(0xC0000064),
    group_exists = static_cast<std::int32_t>(0xC0000065),
    no_such_group = static_cast<std::int32_t>(0xC0000066),
    member_in_group = static_cast<std::int32_t>(0xC0000067),
    member_not_in_group = static_cast<std::int32_t>(0xC0000068),
    wrong_password = static_cast<std::int32_t>(0xC000006A),
    logon_failure = static_cast<std::int32_t>(0xC000006D),
    account_disabled = static_cast<std::int32_t>(0xC0000072),
    not_all_assigned = static_cast<std::int32_t>(0x00000106),
    some_not_mapped = static_cast<std::int32_t>(0x00000107),

    port_connection_refused = static_cast<std::int32_t>(0xC0000041),
    port_disconnected = static_cast<std::int32_t>(0xC0000037),
    invalid_port_handle = static_cast<std::int32_t>(0xC0000042),
    invalid_port_attributes = static_cast<std::int32_t>(0xC000002E),
    port_message_too_long = static_cast<std::int32_t>(0xC000002F),
    pipe_disconnected = static_cast<std::int32_t>(0xC00000B0),
    io_timeout = static_cast<std::int32_t>(0xC00000B5),
    already_disconnected = static_cast<std::int32_t>(0x80000025),

    notify_cleanup = static_cast<std::int32_t>(0x0000010B),
    notify_enum_dir = static_cast<std::int32_t>(0x0000010C),
  };

  struct status {
    using value_type = std::int32_t;

    value_type value{};

    status& operator=(std::int32_t val) noexcept {
      value = val;
      return *this;
    }

    status& operator=(const ntstatus& status) noexcept {
      value = static_cast<std::int32_t>(status);
      return *this;
    }

    [[nodiscard]] constexpr bool operator==(std::int32_t val) const noexcept {
      return value == val;
    }

    [[nodiscard]] constexpr bool operator==(ntstatus status) const noexcept {
      return value == static_cast<std::int32_t>(status);
    }

    [[nodiscard]] constexpr auto operator<=>(const status& other) const noexcept = default;
    [[nodiscard]] constexpr std::strong_ordering operator<=>(std::int32_t val) const noexcept {
      return value <=> val;
    }
    [[nodiscard]] constexpr std::strong_ordering operator<=>(ntstatus status) const noexcept {
      return value <=> static_cast<std::int32_t>(status);
    }

    [[nodiscard]] constexpr bool is_success() const noexcept {
      return value >= 0;
    }

    [[nodiscard]] constexpr bool is_information() const noexcept {
      return severity_bits() == static_cast<std::uint32_t>(omni::severity::information);
    }

    [[nodiscard]] constexpr bool is_warning() const noexcept {
      return severity_bits() == static_cast<std::uint32_t>(omni::severity::warning);
    }

    [[nodiscard]] constexpr bool is_error() const noexcept {
      return severity_bits() == static_cast<std::uint32_t>(omni::severity::error);
    }

    [[nodiscard]] constexpr omni::severity severity() const noexcept {
      return static_cast<omni::severity>(severity_bits());
    }

    [[nodiscard]] constexpr omni::facility facility() const noexcept {
      return static_cast<omni::facility>((bits() >> facility_shift) & facility_mask);
    }

    [[nodiscard]] constexpr std::int32_t code() const noexcept {
      return static_cast<std::int32_t>(bits() & code_mask);
    }

    constexpr explicit operator bool() const noexcept {
      return is_success();
    }

    constexpr explicit operator std::int32_t() const noexcept {
      return value;
    }

   private:
    [[nodiscard]] constexpr std::uint32_t bits() const noexcept {
      return static_cast<std::uint32_t>(value);
    }

    [[nodiscard]] constexpr std::uint32_t severity_bits() const noexcept {
      return (bits() >> severity_shift) & severity_mask;
    }

    constexpr static std::uint32_t severity_shift = 30U;
    constexpr static std::uint32_t severity_mask = 0x3U;
    constexpr static std::uint32_t facility_shift = 16U;
    constexpr static std::uint32_t facility_mask = 0x0FFFU;
    constexpr static std::uint32_t code_mask = 0xFFFFU;
  };

} // namespace omni

template <>
struct std::formatter<omni::status> : std::formatter<omni::status::value_type> {
  auto format(const omni::status& status, std::format_context& ctx) const {
    return std::formatter<omni::status::value_type>::format(status.value, ctx);
  }
};

template <>
struct std::formatter<omni::ntstatus> : std::formatter<std::underlying_type_t<omni::ntstatus>> {
  auto format(const omni::ntstatus& status, std::format_context& ctx) const {
    return std::formatter<std::underlying_type_t<omni::ntstatus>>::format(std::to_underlying(status), ctx);
  }
};

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

#include <system_error>

namespace omni {

  enum class error {
    module_not_loaded = 1,
    export_not_found,
    syscall_id_not_found,
    forwarder_string_invalid,
  };

  namespace detail {
    class error_category : public std::error_category {
     public:
      [[nodiscard]] const char* name() const noexcept override {
#ifdef OMNI_HAS_ERROR_STRINGS
        return "omni.error";
#endif
        return std::system_category().name();
      }

      [[nodiscard]] std::string message([[maybe_unused]] int code) const override {
#ifdef OMNI_HAS_ERROR_STRINGS
        switch (static_cast<omni::error>(code)) {
        case error::module_not_loaded:
          return "module not loaded";
        case error::export_not_found:
          return "export not found";
        case error::syscall_id_not_found:
          return "syscall id not found";
        case error::forwarder_string_invalid:
          return "forwarder string is invalid";
        }
#endif
        return "";
      }
    };
  } // namespace detail

  [[nodiscard]] inline const std::error_category& error_category() noexcept {
    static const detail::error_category category;
    return category;
  }

  [[nodiscard]] inline std::error_code make_error_code(omni::error code) noexcept {
    return {static_cast<int>(code), omni::error_category()};
  }

} // namespace omni

template <>
struct std::is_error_code_enum<omni::error> : std::true_type {};

#include <expected>
#include <utility>

#include <string_view>

namespace omni::detail {

  template <auto Fn>
  consteval std::string_view extract_function_name() {
#if defined(OMNI_COMPILER_CLANG)
    // "... extract_function_name() [Fn = &FunctionNameA]"
    constexpr std::string_view pretty = __PRETTY_FUNCTION__;

    // '&' is where a function name begins
    constexpr auto name_start = pretty.rfind('&') + 1;
    constexpr auto name_end = pretty.find(']');
    constexpr auto func_name = pretty.substr(name_start, name_end - name_start);
#elif defined(OMNI_COMPILER_GCC)
    // "... extract_function_name() [with auto Fn = MessageBoxA; std::string_view = std::basic_string_view<char>]"
    constexpr std::string_view pretty = __PRETTY_FUNCTION__;

    constexpr std::string_view marker{" auto Fn = "};
    constexpr auto name_start = pretty.find(marker) + marker.size();
    constexpr auto name_end = pretty.find(';');
    constexpr auto func_name = pretty.substr(name_start, name_end - name_start);
#elif defined(OMNI_COMPILER_MSVC)
    // "... extract_function_name<int __cdecl A::B::FunctionNameA(int, int*)>(void)"
    constexpr std::string_view sig{__FUNCSIG__};
    constexpr std::string_view marker{"extract_function_name<"};

    constexpr std::size_t after = sig.find(marker) + marker.size(); // "... A::B::FunctionNameA("
    constexpr std::size_t paren = sig.find('(', after);             // '(' of param list
    constexpr auto left_part = sig.substr(after, paren - after);    // "int __cdecl A::B::FunctionNameA"

    // Points to last letter of the name
    constexpr std::size_t name_end = left_part.find_last_not_of(" \t");

    // Last whitespace before the name (space or tab) " A::B::FunctionNameA"
    constexpr std::size_t sep = left_part.find_last_of(" \t", name_end);

    // Begin of the (possibly qualified) identifier
    constexpr std::size_t name_begin = (sep == std::string_view::npos) ? 0 : sep + 1;

    // "A::B::FunctionNameA" or just "FunctionNameA"
    constexpr auto ident = left_part.substr(name_begin, name_end - name_begin + 1);

    // Drop scope qualifier (namespace/class) if present
    // (it will never be the case with WinAPI functions, but anyway...)
    constexpr std::size_t scope = ident.rfind("::");
    constexpr auto func_name = (scope == std::string_view::npos) ? ident : ident.substr(scope + 2);
#else
#  error Unsupported compiler
#endif
    static_assert(!func_name.empty(), "Failed to extract function name");
    return func_name;
  }

} // namespace omni::detail

#include <iterator>
#include <string_view>

namespace omni::detail {

  template <std::size_t N>
  struct fixed_string {
    char value[N]{};

    consteval explicit(false) fixed_string(const char (&str)[N]) {
      for (std::size_t i = 0; i < N; ++i) {
        value[i] = str[i];
      }
    }

    [[nodiscard]] constexpr std::string_view view() const {
      return std::string_view{std::data(value), N - 1};
    }
  };

} // namespace omni::detail

#include <concepts>
#include <cstdint>
#include <iterator>
#include <type_traits>
#include <utility>

namespace omni::detail {

  template <typename T>
  inline auto normalize_pointer_argument(T&& arg) {
    using value_type = std::remove_cvref_t<T>;

    if constexpr (std::is_array_v<value_type>) {
      return std::data(arg);
    } else {
      // All credits to @Debounce, huge thanks to him/her!
      //
      // Since arguments after the fourth are written on the stack,
      // the compiler will fill the lower 32 bits from int with null,
      // and the upper 32 bits will remain undefined.
      //
      // Because the syscall handler expects a (void*)-sized pointer
      // there, this address will be garbage for it, hence AV.
      // If the argument went 1/2/3/4, the compiler would generate a
      // write to ecx/edx/r8d/r9d, by x64 convention, writing to the
      // lower half of a 64-bit register zeroes the upper part too
      // (i.e. ecx = 0 => rcx = 0), so this problem should only exist
      // on x64 for arguments after the fourth.
      // The solution would be on templates to loop through all
      // arguments and manually cast them to size_t size.

      constexpr auto is_signed_integral = std::signed_integral<value_type>;
      constexpr auto is_unsigned_integral = std::unsigned_integral<value_type>;

      using unsigned_integral_type = std::conditional_t<is_unsigned_integral, std::uintptr_t, value_type>;
      using tag_type = std::conditional_t<is_signed_integral, std::intptr_t, unsigned_integral_type>;

      return static_cast<tag_type>(std::forward<T>(arg));
    }
  }

} // namespace omni::detail

#ifdef OMNI_HAS_CACHING

#  include <mutex>
#  include <optional>
#  include <unordered_map>

namespace omni::detail {

  template <typename KeyT, typename ValueT, typename Hasher = std::hash<KeyT>>
  class memory_cache {
   public:
    using hasher = Hasher;
    using key_type = KeyT;
    using value_type = ValueT;
    using storage_type = std::unordered_map<key_type, value_type, hasher>;

    std::optional<value_type> try_get(const key_type& export_hash) {
      std::scoped_lock lock(mutex_);
      auto it = storage_.find(export_hash);
      return it == storage_.end() ? std::nullopt : std::make_optional(it->second);
    }

    void set(const key_type& key, value_type value) {
      std::scoped_lock lock(mutex_);
      storage_[key] = std::move(value);
    }

    void remove(const key_type& key) {
      std::scoped_lock lock(mutex_);
      storage_.erase(key);
    }

    void clear() {
      std::scoped_lock lock(mutex_);
      storage_.clear();
    }

    [[nodiscard]] bool contains(const key_type& key) const {
      std::scoped_lock lock(mutex_);
      return storage_.contains(key);
    }

    [[nodiscard]] std::size_t size() const {
      std::scoped_lock lock(mutex_);
      return storage_.size();
    }

   private:
    mutable std::mutex mutex_;
    storage_type storage_{};
  };

} // namespace omni::detail

#endif

namespace omni {

#ifdef OMNI_HAS_CACHING
  namespace detail {
    struct export_cache_key {
      // Use std::uint64_t as a key to store underlying value type of any hash
      std::uint64_t export_name;
      std::uint64_t module_name;

      [[nodiscard]] auto operator<=>(const export_cache_key&) const noexcept = default;
    };

    struct export_cache_key_hasher {
      [[nodiscard]] std::size_t operator()(const export_cache_key& key) const noexcept {
        std::uint64_t value = key.export_name;
        value ^= std::rotl(key.module_name, 32);
        value ^= 0x9E3779B97F4A7C15ULL;
        value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
        value ^= value >> 31U;
        return static_cast<std::size_t>(value);
      }
    };

    inline detail::memory_cache<export_cache_key, omni::module_export, export_cache_key_hasher> exports_cache;
  } // namespace detail
#endif

  template <typename T>
  class lazy_importer {
   public:
    explicit lazy_importer(concepts::hash auto export_name): export_(resolve_module_export(export_name)) {}
    explicit lazy_importer(default_hash export_name): export_(resolve_module_export(export_name)) {}

    template <concepts::hash Hasher>
    explicit lazy_importer(Hasher export_name, Hasher module_name): export_(resolve_module_export(export_name, module_name)) {}
    explicit lazy_importer(default_hash export_name, default_hash module_name)
      : export_(resolve_module_export(export_name, module_name)) {}

    template <typename... Args>
    std::expected<T, std::error_code> try_invoke(Args&&... args) {
      if (!export_) {
        return std::unexpected(export_.error());
      }

      if constexpr (std::is_void_v<T>) {
        std::ignore = export_->address.template invoke<void>(detail::normalize_pointer_argument(std::forward<Args>(args))...);
        return {};
      } else {
        auto result = export_->address.template invoke<T>(detail::normalize_pointer_argument(std::forward<Args>(args))...);
        return result.value_or(T{});
      }
    }

    template <typename... Args>
    T invoke(Args&&... args) {
      if constexpr (std::is_void_v<T>) {
        std::ignore = try_invoke(std::forward<Args>(args)...);
      } else {
        return try_invoke(std::forward<Args>(args)...).value_or(T{});
      }
    }

    template <typename... Args>
    T operator()(Args&&... args) {
      return invoke(std::forward<Args>(args)...);
    }

    [[nodiscard]] omni::module_export module_export() const noexcept {
      return export_.value_or(omni::module_export{});
    }

   private:
    template <concepts::hash Hasher>
    static std::expected<omni::module_export, std::error_code> resolve_module_export(Hasher export_name, Hasher module_name) {
#ifdef OMNI_HAS_CACHING
      detail::export_cache_key export_cache_key{
        .export_name = export_name.value(),
        .module_name = module_name.value(),
      };

      auto module_export = detail::exports_cache.try_get(export_cache_key);
      if (!module_export or !module_export->present() or !omni::modules{}.contains(module_export->module_base)) {
        omni::module module = omni::get_module(module_name);
        if (!module.present()) {
          return std::unexpected(omni::error::module_not_loaded);
        }

        omni::module_export fresh_export = omni::get_export(export_name, module);
        if (!fresh_export.present()) {
          return std::unexpected(omni::error::export_not_found);
        }

        detail::exports_cache.set(export_cache_key, fresh_export);
        return fresh_export;
      }

      return *module_export;
#else
      omni::module module = omni::get_module(module_name);
      if (!module.present()) {
        return std::unexpected(omni::error::module_not_loaded);
      }

      omni::module_export fresh_export = omni::get_export(export_name, module);
      if (!fresh_export.present()) {
        return std::unexpected(omni::error::export_not_found);
      }

      return fresh_export;
#endif
    }

    static std::expected<omni::module_export, std::error_code> resolve_module_export(concepts::hash auto export_name) {
#ifdef OMNI_HAS_CACHING
      detail::export_cache_key export_cache_key{.export_name = export_name.value()};
      auto module_export = detail::exports_cache.try_get(export_cache_key);

      // The export is missing from the cache, or its owning module was
      // unloaded. If the module is loaded again at a different base, we
      // refresh the cached export, this adds a fast O(n) loaded-module
      // check to each export lookup to detect stale cache entries
      if (!module_export or !module_export->present() or !omni::modules{}.contains(module_export->module_base)) {
        omni::module_export fresh_export = omni::get_export(export_name);
        if (!fresh_export.present()) {
          return std::unexpected(omni::error::export_not_found);
        }

        detail::exports_cache.set(export_cache_key, fresh_export);
        return fresh_export;
      }

      return *module_export;
#else
      omni::module_export fresh_export = omni::get_export(export_name);
      if (!fresh_export.present()) {
        return std::unexpected(omni::error::export_not_found);
      }

      return fresh_export;
#endif
    }

    std::expected<omni::module_export, std::error_code> export_;
  };

  template <typename T, typename... Params>
  class lazy_importer<T (*)(Params...)> : private lazy_importer<T> {
   public:
    using lazy_importer<T>::lazy_importer;
    using lazy_importer<T>::module_export;

    std::expected<T, std::error_code> try_invoke(Params... args) {
      return lazy_importer<T>::try_invoke(args...);
    }

    T invoke(Params... args) {
      return lazy_importer<T>::invoke(args...);
    }

    T operator()(Params... args) {
      return lazy_importer<T>::invoke(args...);
    }
  };

#if defined(OMNI_ARCH_X86)
  template <typename T, typename... Params>
  class lazy_importer<T(__stdcall*)(Params...)> : private lazy_importer<T> {
   public:
    using lazy_importer<T>::lazy_importer;
    using lazy_importer<T>::module_export;

    std::expected<T, std::error_code> try_invoke(Params... args) {
      return lazy_importer<T>::try_invoke(args...);
    }

    T invoke(Params... args) {
      return lazy_importer<T>::invoke(args...);
    }

    T operator()(Params... args) {
      return lazy_importer<T>::invoke(args...);
    }
  };
#endif

  template <typename T = void, concepts::hash Hasher, typename... Args>
  inline T lazy_import(Hasher export_name, Args&&... args) {
    return lazy_importer<T>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <typename T = void, typename... Args>
  inline T lazy_import(default_hash export_name, Args&&... args) {
    return lazy_import<T, default_hash>(export_name, std::forward<Args>(args)...);
  }

  template <typename T = void, concepts::hash Hasher, typename... Args>
  inline T lazy_import(omni::hash_pair<Hasher> export_and_module_names, Args&&... args) {
    auto [export_name, module_name] = export_and_module_names;
    return lazy_importer<T>{export_name, module_name}.invoke(std::forward<Args>(args)...);
  }

  template <typename T = void, typename... Args>
  inline T lazy_import(omni::hash_pair<> export_and_module_names, Args&&... args) {
    return lazy_import<T, default_hash>(export_and_module_names, std::forward<Args>(args)...);
  }

  template <auto Func, concepts::hash Hasher, class... Args>
  inline auto lazy_import(Args&&... args) {
    constexpr Hasher func_name{detail::extract_function_name<Func>()};
    return lazy_importer<decltype(Func)>{func_name}.invoke(std::forward<Args>(args)...);
  }

  template <auto Func, class... Args>
  inline auto lazy_import(Args&&... args) {
    return lazy_import<Func, default_hash>(std::forward<Args>(args)...);
  }

  template <auto Func, detail::fixed_string ModuleName, concepts::hash Hasher, class... Args>
  inline auto lazy_import(Args&&... args) {
    constexpr Hasher func_name{detail::extract_function_name<Func>()};
    constexpr Hasher module_name{ModuleName.view()};
    return lazy_importer<decltype(Func)>{func_name, module_name}.invoke(std::forward<Args>(args)...);
  }

  template <auto Func, detail::fixed_string ModuleName, class... Args>
  inline auto lazy_import(Args&&... args) {
    return lazy_import<Func, ModuleName, default_hash>(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, concepts::hash Hasher, class... Args>
  inline auto lazy_import(Hasher export_name, Args&&... args) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    return lazy_importer<F>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, class... Args>
  inline auto lazy_import(default_hash export_name, Args&&... args) {
    return lazy_import<F, default_hash>(export_name, std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, concepts::hash Hasher, class... Args>
  inline auto lazy_import(omni::hash_pair<Hasher> export_and_module_names, Args&&... args) {
    auto [export_name, module_name] = export_and_module_names;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay,hicpp-no-array-decay)
    return lazy_importer<F>{export_name, module_name}.invoke(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, class... Args>
  inline auto lazy_import(omni::hash_pair<> export_and_module_names, Args&&... args) {
    return lazy_import<F, default_hash>(export_and_module_names, std::forward<Args>(args)...);
  }

} // namespace omni

#include <cstdint>
#include <filesystem>

namespace omni::win {

  struct kernel_system_time {
    std::uint32_t low_part;
    std::int32_t high1_time;
    std::int32_t high2_time;
  };

  enum nt_product_type {
    win_nt = 1,
    lan_man_nt = 2,
    server = 3
  };

  enum alternative_arch_type {
    standart_design,
    nec98x86,
    end_alternatives
  };

  struct xstate_feature {
    std::uint32_t offset;
    std::uint32_t size;
  };

  struct xstate_configuration {
    // Mask of all enabled features
    std::uint64_t enabled_features;
    // Mask of volatile enabled features
    std::uint64_t enabled_volatile_features;
    // Total size of the save area for user states
    std::uint32_t size;
    // Control Flags
    union {
      std::uint32_t control_flags;
      struct {
        std::uint32_t optimized_save:1;
        std::uint32_t compaction_enabled:1;
        std::uint32_t extended_feature_disable:1;
      };
    };
    // List of features
    xstate_feature features[64];
    // Mask of all supervisor features
    std::uint64_t enabled_supervisor_features;
    // Mask of features that require start address to be 64 byte aligned
    std::uint64_t aligned_features;
    // Total size of the save area for user and supervisor states
    std::uint32_t all_features_size;
    // List which holds size of each user and supervisor state supported by CPU
    std::uint32_t all_features[64];
    // Mask of all supervisor features that are exposed to user-mode
    std::uint64_t enabled_user_visible_supervisor_features;
    // Mask of features that can be disabled via XFD
    std::uint64_t extended_feature_disable_features;
    // Total size of the save area for non-large user and supervisor states
    std::uint32_t all_non_large_feature_size;
    std::uint32_t spare;
  };

  union win32_large_integer {
    struct {
      std::uint32_t low_part;
      std::int32_t high_part;
    };
    struct {
      std::uint32_t low_part;
      std::int32_t high_part;
    } u;
    std::uint64_t quad_part;
  };

  struct kernel_user_shared_data {
    std::uint32_t tick_count_low_deprecated;
    std::uint32_t tick_count_multiplier;
    kernel_system_time interrupt_time;
    kernel_system_time system_time;
    kernel_system_time time_zone_bias;
    std::uint16_t image_number_low;
    std::uint16_t image_number_high;
    wchar_t nt_system_root[260];
    std::uint32_t max_stack_trace_depth;
    std::uint32_t crypto_exponent;
    std::uint32_t time_zone_id;
    std::uint32_t large_page_minimum;
    std::uint32_t ait_sampling_value;
    std::uint32_t app_compat_flag;
    std::uint64_t random_seed_version;
    std::uint32_t global_validation_runlevel;
    std::int32_t time_zone_bias_stamp;
    std::uint32_t nt_build_number;
    nt_product_type nt_product_type;
    bool product_type_is_valid;
    bool reserved0[1];
    std::uint16_t native_processor_architecture;
    std::uint32_t nt_major_version;
    std::uint32_t nt_minor_version;
    bool processor_features[64];
    std::uint32_t reserved1;
    std::uint32_t reserved3;
    std::uint32_t time_slip;
    alternative_arch_type alternative_arch;
    std::uint32_t boot_id;
    win32_large_integer system_expiration_date;
    std::uint32_t suite_mask;
    bool kernel_debugger_enabled;
    union {
      std::uint8_t mitigation_policies;
      struct {
        std::uint8_t nx_support_policy:2;
        std::uint8_t seh_validation_policy:2;
        std::uint8_t cur_dir_devices_skipped_for_modules:2;
        std::uint8_t reserved:2;
      };
    };
    std::uint16_t cycles_per_yield;
    std::uint32_t active_console_id;
    std::uint32_t dismount_count;
    std::uint32_t com_plus_package;
    std::uint32_t last_system_rit_event_tick_count;
    std::uint32_t number_of_physical_pages;
    bool safe_boot_mode;
    union {
      std::uint8_t virtualization_flags;
      struct {
        std::uint8_t arch_started_in_el2:1;
        std::uint8_t qc_sl_is_supported:1;
      };
    };
    std::uint8_t reserved12[2];
    union {
      std::uint32_t shared_data_flags;
      struct {
        std::uint32_t dbg_error_port_present:1;
        std::uint32_t dbg_elevation_enabled:1;
        std::uint32_t dbg_virt_enabled:1;
        std::uint32_t dbg_installer_detect_enabled:1;
        std::uint32_t dbg_lkg_enabled:1;
        std::uint32_t dbg_dyn_processor_enabled:1;
        std::uint32_t dbg_console_broker_enabled:1;
        std::uint32_t dbg_secure_boot_enabled:1;
        std::uint32_t dbg_multi_session_sku:1;
        std::uint32_t dbg_multi_users_in_session_sku:1;
        std::uint32_t dbg_state_separation_enabled:1;
        std::uint32_t spare_bits:21;
      };
    };
    std::uint32_t data_flags_pad[1];
    std::uint64_t test_ret_instruction;
    std::int64_t qpc_frequency;
    std::uint32_t system_call;
    std::uint32_t reserved2;
    std::uint64_t full_number_of_physical_pages;
    std::uint64_t system_call_pad[1];
    union {
      kernel_system_time tick_count;
      std::uint64_t tick_count_quad;
      struct {
        std::uint32_t reserved_tick_count_overlay[3];
        std::uint32_t tick_count_pad[1];
      };
    };
    std::uint32_t cookie;
    std::uint32_t cookie_pad[1];
    std::int64_t console_session_foreground_process_id;
    std::uint64_t time_update_lock;
    std::uint64_t baseline_system_time_qpc;
    std::uint64_t baseline_interrupt_time_qpc;
    std::uint64_t qpc_system_time_increment;
    std::uint64_t qpc_interrupt_time_increment;
    std::uint8_t qpc_system_time_increment_shift;
    std::uint8_t qpc_interrupt_time_increment_shift;
    std::uint16_t unparked_processor_count;
    std::uint32_t enclave_feature_mask[4];
    std::uint32_t telemetry_coverage_round;
    std::uint16_t user_mode_global_logger[16];
    std::uint32_t image_file_execution_options;
    std::uint32_t lang_generation_count;
    std::uint64_t reserved4;
    std::uint64_t interrupt_time_bias;
    std::uint64_t qpc_bias;
    std::uint32_t active_processor_count;
    std::uint8_t active_group_count;
    std::uint8_t reserved9;
    union {
      std::uint16_t qpc_data;
      struct {
        std::uint8_t qpc_bypass_enabled;
        std::uint8_t qpc_reserved;
      };
    };
    win32_large_integer time_zone_bias_effective_start;
    win32_large_integer time_zone_bias_effective_end;
    xstate_configuration xstate;
    kernel_system_time feature_configuration_change_stamp;
    std::uint32_t spare;
    std::uint64_t user_pointer_auth_mask;
    xstate_configuration xstate_arm64;
    std::uint32_t reserved10[210];

    [[nodiscard]] std::filesystem::path system_root(
      std::filesystem::path::format fmt = std::filesystem::path::auto_format) const {
      return {nt_system_root, fmt};
    }
  };

} // namespace omni::win

namespace omni {

  [[nodiscard]] inline omni::win::kernel_user_shared_data* shared_user_data() noexcept {
    constexpr static omni::address memory_location{0x7ffe0000};
    return memory_location.ptr<win::kernel_user_shared_data>();
  }

} // namespace omni

#include <expected>
#include <system_error>
#include <utility>

#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace omni::detail {

  template <typename T, typename... Args>
  class inplace_function;

  template <typename T, typename... Args>
  class inplace_function<T(Args...)> {
   public:
    using function_ptr_t = T (*)(void*, Args&&...);
    using copy_ptr_t = void (*)(void*, const void*);
    using move_ptr_t = void (*)(void*, void*);
    using destructor_ptr_t = void (*)(void*);

    constexpr static std::size_t storage_size = 64;
    constexpr static std::size_t storage_alignment = alignof(std::max_align_t);

    inplace_function() = default;

    template <typename F, typename DecayedFunction = std::decay_t<F>>
      requires(!std::same_as<DecayedFunction, inplace_function> && std::is_invocable_r_v<T, DecayedFunction&, Args...> &&
               std::is_copy_constructible_v<DecayedFunction>)
    explicit(false) inplace_function(F&& function_object) {
      emplace<DecayedFunction>(std::forward<F>(function_object));
    }

    inplace_function(const inplace_function& other) {
      copy_from(other);
    }

    inplace_function(inplace_function&& other) { // NOLINT(*-noexcept-move*)
      move_from(other);
    }

    inplace_function& operator=(const inplace_function& other) {
      if (this == &other) {
        return *this;
      }

      if (!other) {
        reset();
        return *this;
      }

      inplace_function temp(other);
      reset();
      move_from(temp);
      return *this;
    }

    inplace_function& operator=(inplace_function&& other) { // NOLINT(*-noexcept-move*)
      if (this == &other) {
        return *this;
      }

      reset();
      move_from(other);
      return *this;
    }

    ~inplace_function() {
      reset();
    }

    void swap(inplace_function& other) { // NOLINT(*-noexcept-swap)
      if (this == &other) {
        return;
      }

      inplace_function temp;
      temp.move_from(*this);
      move_from(other);
      other.move_from(temp);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
      return invoker_ != nullptr;
    }

    T operator()(Args... args) {
      return invoke(std::forward<Args>(args)...);
    }

    T operator()(Args... args) const {
      return invoke(std::forward<Args>(args)...);
    }

   private:
    template <typename F>
    static F* storage_as(void* ptr) noexcept {
      return std::launder(reinterpret_cast<F*>(ptr));
    }

    template <typename F>
    static const F* storage_as(const void* ptr) noexcept {
      return std::launder(reinterpret_cast<const F*>(ptr));
    }

    [[nodiscard]] void* storage_ptr() noexcept {
      return static_cast<void*>(storage_);
    }

    [[nodiscard]] const void* storage_ptr() const noexcept {
      return static_cast<const void*>(storage_);
    }

    template <typename... CallArgs>
    [[nodiscard]] T invoke(CallArgs&&... args) const {
      if (invoker_ == nullptr) {
#ifdef OMNI_HAS_EXCEPTIONS
        throw std::bad_function_call();
#else
        std::abort();
#endif
      }

      return invoker_(const_cast<void*>(storage_ptr()), std::forward<CallArgs>(args)...);
    }

    template <typename F, typename... CtorArgs>
    void emplace(CtorArgs&&... args) {
      static_assert(sizeof(F) <= storage_size, "Function object too large");
      static_assert(alignof(F) <= storage_alignment, "Function object alignment too large");

      new (storage_ptr()) F(std::forward<CtorArgs>(args)...);

      invoker_ = [](void* ptr, Args&&... args) -> T {
        return std::invoke(*storage_as<F>(ptr), std::forward<Args>(args)...);
      };

      copier_ = [](void* destination, const void* source) {
        new (destination) F(*storage_as<F>(source));
      };

      mover_ = [](void* destination, void* source) {
        if constexpr (std::is_move_constructible_v<F>) {
          new (destination) F(std::move(*storage_as<F>(source)));
        } else {
          new (destination) F(*storage_as<F>(source));
        }

        std::destroy_at(storage_as<F>(source));
      };

      destroyer_ = [](void* ptr) {
        std::destroy_at(storage_as<F>(ptr));
      };
    }

    void copy_from(const inplace_function& other) {
      if (!other) {
        return;
      }

      other.copier_(storage_ptr(), other.storage_ptr());
      invoker_ = other.invoker_;
      copier_ = other.copier_;
      mover_ = other.mover_;
      destroyer_ = other.destroyer_;
    }

    void move_from(inplace_function& other) {
      if (!other) {
        return;
      }

      other.mover_(storage_ptr(), other.storage_ptr());
      invoker_ = other.invoker_;
      copier_ = other.copier_;
      mover_ = other.mover_;
      destroyer_ = other.destroyer_;
      other.clear();
    }

    void reset() {
      if (destroyer_ != nullptr) {
        destroyer_(storage_ptr());
      }

      clear();
    }

    void clear() noexcept {
      invoker_ = nullptr;
      copier_ = nullptr;
      mover_ = nullptr;
      destroyer_ = nullptr;
    }

    alignas(storage_alignment) std::byte storage_[storage_size]{};
    function_ptr_t invoker_{nullptr};
    copy_ptr_t copier_{nullptr};
    move_ptr_t mover_{nullptr};
    destructor_ptr_t destroyer_{nullptr};
  };

} // namespace omni::detail

#include <array>
#include <cstring>

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

namespace omni {

  namespace detail {
#ifdef OMNI_HAS_CACHING
    // Use std::uint64_t as a key to store underlying value type of any hash
    inline detail::memory_cache<std::uint64_t, std::uint32_t> syscall_id_cache;
#endif
  } // namespace detail

  // Syscall ID is at an offset of 4 bytes from the specified address.
  // Not considering the situation when EDR hook is installed
  // Learn more here: https://github.com/annihilatorq/shadow_syscall/issues/1
  inline std::expected<std::uint32_t, std::error_code> default_syscall_id_parser(const omni::module_export& module_export) {
    auto* address = module_export.address.ptr<std::uint8_t>();

    for (std::size_t i{}; i < 24; ++i) {
      if (address[i] == 0x4c && address[i + 1] == 0x8b && address[i + 2] == 0xd1 && address[i + 3] == 0xb8 &&
          address[i + 6] == 0x00 && address[i + 7] == 0x00) {
        return *reinterpret_cast<std::uint32_t*>(&address[i + 4]);
      }
    }

    return std::unexpected(omni::error::syscall_id_not_found);
  }

  template <typename T = omni::status>
    requires(omni::detail::is_x64)
  class syscaller {
   public:
    using syscall_id_parser = detail::inplace_function<std::expected<std::uint32_t, std::error_code>(omni::module_export)>;

    explicit syscaller(concepts::hash auto export_name, syscall_id_parser parser = default_syscall_id_parser)
      : syscall_id_parser_(std::move(parser)), syscall_id_(resolve_syscall_id(export_name)) {
      if (syscall_id_) {
        setup_shellcode(*syscall_id_);
      }
    }

    explicit syscaller(default_hash export_name, syscall_id_parser parser = default_syscall_id_parser)
      : syscall_id_parser_(std::move(parser)), syscall_id_(resolve_syscall_id(export_name)) {
      if (syscall_id_) {
        setup_shellcode(*syscall_id_);
      }
    }

    template <typename... Args>
    std::expected<T, std::error_code> try_invoke(Args&&... args) {
      if (!syscall_id_) {
        return std::unexpected(syscall_id_.error());
      }
      if constexpr (std::is_void_v<T>) {
        shellcode_.execute(detail::normalize_pointer_argument(args)...);
        return {};
      } else {
        return shellcode_.execute<T>(detail::normalize_pointer_argument(args)...);
      }
    }

    template <typename... Args>
    T invoke(Args&&... args) {
      if constexpr (std::is_void_v<T>) {
        std::ignore = try_invoke(std::forward<Args>(args)...);
      } else {
        return try_invoke(std::forward<Args>(args)...).value_or(T{});
      }
    }

    template <typename... Args>
    T operator()(Args&&... args) {
      return invoke(std::forward<Args>(args)...);
    }

   private:
    void setup_shellcode(std::uint32_t syscall_id) {
      shellcode_.write<std::uint32_t>(6, syscall_id);
      shellcode_.setup();
    }

    std::expected<std::uint32_t, std::error_code> resolve_syscall_id(concepts::hash auto export_name) {
#ifdef OMNI_HAS_CACHING
      auto cached_syscall_id = detail::syscall_id_cache.try_get(export_name.value());
      if (cached_syscall_id.has_value()) {
        return cached_syscall_id.value();
      }
#endif
      omni::module_export module_export = omni::get_export(export_name);
      if (!module_export.present()) {
        return std::unexpected(omni::error::export_not_found);
      }

      auto parsed_syscall_id = syscall_id_parser_(module_export);
      if (!parsed_syscall_id) {
        return std::unexpected(parsed_syscall_id.error());
      }

#ifdef OMNI_HAS_CACHING
      detail::syscall_id_cache.set(export_name.value(), *parsed_syscall_id);
#endif

      return *parsed_syscall_id;
    }

    syscall_id_parser syscall_id_parser_;
    std::expected<std::uint32_t, std::error_code> syscall_id_;

    detail::shellcode<13> shellcode_{{0x49, 0x89, 0xCA, 0x48, 0xC7, 0xC0, 0x3F, 0x10, 0x00, 0x00, 0x0F, 0x05, 0xC3}};
  };

  template <typename T, typename... Params>
    requires(omni::detail::is_x64)
  class syscaller<T (*)(Params...)> : public syscaller<T> {
   public:
    using syscaller<T>::syscaller;

    std::expected<T, std::error_code> try_invoke(Params... args) {
      return syscaller<T>::try_invoke(args...);
    }

    T invoke(Params... args) {
      return syscaller<T>::invoke(args...);
    }

    T operator()(Params... args) {
      return syscaller<T>::invoke(args...);
    }
  };

  template <typename T = omni::status, concepts::hash Hasher, typename... Args>
    requires(!concepts::function_pointer<T>)
  inline T syscall(Hasher export_name, Args&&... args) {
    return syscaller<T>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <typename T = omni::status, typename... Args>
    requires(!concepts::function_pointer<T>)
  inline T syscall(default_hash export_name, Args&&... args) {
    return syscall<T, default_hash>(export_name, std::forward<Args>(args)...);
  }

  template <auto Func, concepts::hash Hasher, class... Args>
  inline auto syscall(Args&&... args) {
    constexpr Hasher func_name{detail::extract_function_name<Func>()};
    return syscaller<decltype(Func)>{func_name}.invoke(std::forward<Args>(args)...);
  }

  template <auto Func, class... Args>
  inline auto syscall(Args&&... args) {
    return syscall<Func, default_hash>(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, concepts::hash Hasher, class... Args>
  inline auto syscall(Hasher export_name, Args&&... args) {
    return syscaller<F>{export_name}.invoke(std::forward<Args>(args)...);
  }

  template <concepts::function_pointer F, class... Args>
  inline auto syscall(default_hash export_name, Args&&... args) {
    return syscall<F, default_hash>(export_name, std::forward<Args>(args)...);
  }

} // namespace omni
