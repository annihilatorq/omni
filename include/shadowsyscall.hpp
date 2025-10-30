// [FAQ / Examples] here: https://github.com/annihilatorq/shadow_syscall

// Creator Discord - @ntraiseharderror,
// Telegram - https://t.me/annihilatorq,
// Github - https://github.com/annihilatorq

// Credits to https://github.com/can1357/linux-pe for the very pretty structs
// Special thanks to @inversion

#ifndef SHADOW_SYSCALL_HPP
#define SHADOW_SYSCALL_HPP

#ifndef SHADOWSYSCALLS_DISABLE_CACHING
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#endif

#include <intrin.h>
#include <math.h>
#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#if defined(__cpp_exceptions) && __cpp_exceptions
#define SHADOW_HAS_EXCEPTIONS true
#else
#define SHADOW_HAS_EXCEPTIONS false
#endif

namespace shadow::concepts {

  template <typename Ty>
  concept arithmetic = std::is_arithmetic_v<Ty>;
  template <typename Ty>
  concept pointer = std::is_pointer_v<Ty>;
  template <typename Ty>
  concept nullpointer = std::is_null_pointer_v<Ty>;
  template <typename Ty>
  concept indexable_sizeable = requires(Ty t) {
    { t.size() } -> std::convertible_to<std::size_t>;
    { t[0] } -> std::convertible_to<typename Ty::value_type>;
  };
  template <typename Ty>
  concept hashable = indexable_sizeable<Ty>;
  template <typename Ty>
  concept chrono_duration =
      std::is_base_of_v<std::chrono::duration<typename Ty::rep, typename Ty::period>, Ty>;
  template <typename Ty>
  concept function_type = std::is_pointer_v<Ty> && std::is_function_v<std::remove_pointer_t<Ty>>;

  template <typename Ty>
  concept fundamental = [] {
    static_assert(std::is_fundamental_v<Ty>,
                  "Nt/Zw functions cannot return the type you specified."
                  "Type should be fundamental.");
    return true;
  }();

  template <typename>
  struct function_traits;

#if defined(_MSC_VER) && _WIN32
  // I'm tired. I give up. I don't know how to overload
  // calling conventions in C++, so on x86 MSVC we will
  // only support the WinAPI convention; functions with
  // __cdecl (CRT) conventions on x86 will not be
  // available for typed calls. PRs are welcome.
  template <typename Ret, typename... Args>
  struct function_traits<Ret(__stdcall*)(Args...)>
#else
  template <typename Ret, typename... Args>
  struct function_traits<Ret (*)(Args...)>
#endif
  {
    using return_type = Ret;
    using argument_types = std::tuple<Args...>;

    template <std::size_t N>
    using argument_type = std::tuple_element_t<N, argument_types>;

    static constexpr std::size_t arity = sizeof...(Args);
    static constexpr bool is_void = std::is_void_v<Ret>;
  };

#ifdef SHADOW_RELAXED_POINTER_COMPAT
  constexpr bool allow_any_ptr_for_ptr_param = true;  // U* -> T*
  constexpr bool allow_voidptr_for_ptr_param = true;  // void* -> T*
#else
  constexpr bool allow_any_ptr_for_ptr_param = false;
  constexpr bool allow_voidptr_for_ptr_param = false;
#endif

  template <typename ParamType, typename ArgType>
  struct arg_compatible {
    using ParamNoCVT = std::remove_cv_t<ParamType>;
    using ArgDecayedT = std::decay_t<ArgType>;
    using ArgNoRefT = std::remove_reference_t<ArgType>;

    static constexpr bool is_pointer_compatible() {
      if constexpr (std::is_null_pointer_v<ArgDecayedT>)
        return true;

      // let arrays pass, since string-literal is const char[N],
      // they will be decayed to a const char* after
      if constexpr (std::is_array_v<ArgNoRefT>)
        return true;

      if constexpr (std::is_pointer_v<ArgDecayedT>) {
        if constexpr (allow_any_ptr_for_ptr_param)
          return true;

        if constexpr (allow_voidptr_for_ptr_param && std::is_same_v<ArgDecayedT, void*>)
          return true;

        using ParamPointeeT = std::remove_cv_t<std::remove_pointer_t<ParamNoCVT>>;
        using ArgPointeeT = std::remove_cv_t<std::remove_pointer_t<ArgDecayedT>>;
        return std::is_same_v<ParamPointeeT, ArgPointeeT>;
      }

      if constexpr (std::is_integral_v<ArgDecayedT>)  // allow passing integer-zero for a pointer
        return true;

      return false;
    }

    static constexpr bool value = std::is_pointer_v<ParamNoCVT>
                                      ? is_pointer_compatible()
                                      : std::is_convertible_v<ArgDecayedT, ParamNoCVT>;
  };

  template <typename F, typename... Args>
  struct args_compatible {
    using traits = function_traits<F>;
    static constexpr bool count_ok = (sizeof...(Args) == traits::arity);

    template <std::size_t... I>
    static constexpr bool check(std::index_sequence<I...>) {
      return (arg_compatible<typename traits::template argument_type<I>, Args>::value && ...);
    }

    static constexpr bool value = count_ok && check(std::make_index_sequence<traits::arity>{});
  };

  template <typename F, typename... Args>
  inline constexpr bool args_compatible_v = args_compatible<F, Args...>::value;

  template <typename Ty>
  using non_void_t = std::conditional_t<std::is_void_v<Ty>, std::monostate, Ty>;

  template <typename Ty>
  constexpr bool is_type_ntstatus = std::is_same_v<std::remove_cv_t<Ty>, long>;

}  // namespace shadow::concepts

namespace shadow {

  [[maybe_unused]] constexpr auto bitness = std::numeric_limits<uintptr_t>::digits;
  [[maybe_unused]] constexpr auto is_x64 = bitness == 64;
  [[maybe_unused]] constexpr auto is_x32 = bitness == 32;

  class address_t {
   public:
    using underlying_t = std::uintptr_t;

    constexpr address_t() = default;
    // this way, we don't get the `ambiguous_condition` error
    constexpr address_t(concepts::nullpointer auto) noexcept {}
    constexpr address_t(underlying_t address) noexcept : m_address(address) {}
    constexpr address_t(concepts::pointer auto address) noexcept
        : m_address(reinterpret_cast<underlying_t>(address)) {}
    constexpr address_t(std::ranges::contiguous_range auto range) noexcept
        : m_address(reinterpret_cast<underlying_t>(range.data())) {}

    address_t(const address_t&) = default;
    address_t(address_t&&) = default;
    address_t& operator=(const address_t&) = default;
    address_t& operator=(address_t&&) = default;
    ~address_t() = default;

    template <typename Ty = void, typename PointerTy = std::add_pointer_t<Ty>>
    [[nodiscard]] constexpr PointerTy ptr(std::ptrdiff_t offset = 0) const noexcept {
      return this->offset(offset).as<PointerTy>();
    }

    [[nodiscard]] constexpr underlying_t get() const noexcept {
      return m_address;
    }

    template <typename Ty = address_t>
    [[nodiscard]] constexpr Ty offset(std::ptrdiff_t offset = 0) const noexcept {
      if constexpr (std::is_pointer_v<Ty>)
        return m_address == 0u ? nullptr : reinterpret_cast<Ty>(m_address + offset);
      else
        return m_address == 0u ? static_cast<Ty>(*this) : Ty{m_address + offset};
    }

    template <typename Ty>
    [[nodiscard]] constexpr Ty as() const noexcept {
      if constexpr (std::is_pointer_v<Ty>)
        return reinterpret_cast<Ty>(m_address);
      else
        return static_cast<Ty>(m_address);
    }

    template <typename Ty>
    [[nodiscard]] constexpr std::span<Ty> span(std::size_t count) const noexcept {
      return {this->ptr<Ty>(), count};
    }

    [[nodiscard]] bool is_in_range(const address_t& start, const address_t& end) const noexcept {
      return (*this >= start) && (*this < end);
    }

    template <typename Ty, typename... Args>
    [[nodiscard]] Ty execute(Args&&... args) const noexcept {
      if (m_address == 0) {
        if constexpr (std::is_pointer_v<Ty>)
          return nullptr;
        else
          return Ty{};
      }

      using target_function_t = Ty(__stdcall*)(std::decay_t<Args>...);
      const auto target_function = reinterpret_cast<target_function_t>(m_address);

      return target_function(std::forward<Args>(args)...);
    }

    constexpr explicit operator std::uintptr_t() const noexcept {
      return m_address;
    }
    constexpr explicit operator bool() const noexcept {
      return static_cast<bool>(m_address);
    }
    constexpr auto operator<=>(const address_t&) const = default;

    constexpr address_t operator+=(const address_t& rhs) noexcept {
      m_address += rhs.m_address;
      return *this;
    }

    constexpr address_t operator-=(const address_t& rhs) noexcept {
      m_address -= rhs.m_address;
      return *this;
    }

    [[nodiscard]] constexpr address_t operator+(const address_t& rhs) const noexcept {
      return {m_address + rhs.m_address};
    }

    [[nodiscard]] constexpr address_t operator-(const address_t& rhs) const noexcept {
      return {m_address - rhs.m_address};
    }

    [[nodiscard]] constexpr address_t operator&(const address_t& other) const noexcept {
      return {m_address & other.m_address};
    }

    [[nodiscard]] constexpr address_t operator|(const address_t& other) const noexcept {
      return {m_address | other.m_address};
    }

    [[nodiscard]] constexpr address_t operator^(const address_t& other) const noexcept {
      return {m_address ^ other.m_address};
    }

    [[nodiscard]] constexpr address_t operator<<(std::size_t shift) const noexcept {
      return {m_address << shift};
    }

    [[nodiscard]] constexpr address_t operator>>(std::size_t shift) const noexcept {
      return {m_address >> shift};
    }

    friend std::ostream& operator<<(std::ostream& os, const address_t& address) {
      return os << address.ptr();
    }

   private:
    underlying_t m_address{0};
  };

  namespace win {
    constexpr static std::uint32_t num_data_directories = 16;
    constexpr static std::uint32_t img_npos = 0xFFFFFFFF;

    inline auto split_forwarder_string(std::string_view view, char delimiter) noexcept {
      auto pos = view.find(delimiter);
      if (pos != std::string_view::npos) {
        auto first_part = view.substr(0, pos);
        auto second_part = view.substr(pos + 1);
        return std::pair{first_part, second_part};
      }
      return std::pair{view, std::string_view{}};
    }

    union version_t {
      uint16_t identifier;
      struct {
        uint8_t major;
        uint8_t minor;
      };
    };

    union ex_version_t {
      uint32_t identifier;
      struct {
        uint16_t major;
        uint16_t minor;
      };
    };

    struct forwarder_string {
      std::string_view dll;
      std::string_view function;
      [[nodiscard]] bool present() const noexcept {
        return !(dll.empty() || function.empty());
      }
    };

    struct section_string_t {
      char short_name[8];

      [[nodiscard]] auto view() const noexcept {
        return std::string_view{short_name};
      }
      [[nodiscard]] explicit operator std::string_view() const noexcept {
        return view();
      }
      [[nodiscard]] auto operator[](size_t n) const noexcept {
        return view()[n];
      }
      [[nodiscard]] bool operator==(const section_string_t& other) const {
        return view().compare(other.view()) == 0;
      }
    };

    struct unicode_string {
      using char_t = wchar_t;
      using pointer_t = char_t*;

     public:
      constexpr unicode_string() = default;
      constexpr unicode_string(pointer_t buffer, std::uint16_t length,
                               std::uint16_t max_length = 0) noexcept
          : m_length(length), m_max_length(max_length), m_buffer(buffer) {}

      unicode_string(const unicode_string& instance) = default;
      unicode_string(unicode_string&& instance) = default;
      unicode_string& operator=(const unicode_string& instance) = default;
      unicode_string& operator=(unicode_string&& instance) = default;
      ~unicode_string() = default;

      template <typename Ty>
        requires(std::is_constructible_v<Ty, pointer_t>)
      [[nodiscard]] auto as() const noexcept(std::is_nothrow_constructible_v<Ty, pointer_t>) {
        return Ty{m_buffer};
      }

      [[nodiscard]] auto
      to_path(std::filesystem::path::format fmt = std::filesystem::path::auto_format) const {
        return std::filesystem::path{m_buffer, fmt};
      }

      [[nodiscard]] auto view() const noexcept {
        return std::wstring_view{m_buffer, m_length / sizeof(char_t)};
      }

      [[nodiscard]] auto string() const {
        // \note: Since std::codecvt & std::wstring_convert is
        // deprecated in cpp17 and will be deleted in
        // cpp26, we use the std::filesystem::path
        // as a string converter, although it will
        // require more memory, we will be sure
        // that the conversion will be correct.
        // We will not use wcstombs_s because of the
        // dependency on the current locale.

        const auto source_str = view();
        const auto has_non_ascii_symbols = contains_non_ascii(source_str);
        if (has_non_ascii_symbols) {
          // Use std::filesystem::path as string converter
          return to_path().string();
        } else {
          // Otherwise, return string_view converted to std::string
          return std::string(source_str.begin(), source_str.end());
        }
      }

      [[nodiscard]] auto data() const noexcept {
        return m_buffer;
      }
      [[nodiscard]] auto size() const noexcept {
        return m_length;
      }
      [[nodiscard]] auto empty() const noexcept {
        return m_buffer == nullptr || m_length == 0;
      }

      [[nodiscard]] bool operator==(const unicode_string& right) const noexcept {
        return m_buffer == right.m_buffer && m_length == right.m_length;
      }

      [[nodiscard]] bool operator==(std::wstring_view right) const noexcept {
        return view() == right;
      }

      [[nodiscard]] bool operator==(std::string_view right) const noexcept {
        const auto src_view = view();
        if (src_view.size() != right.size())
          return false;

        auto wide_string_transformed_to_ascii = std::ranges::transform_view(
            src_view, [](wchar_t wc) -> char { return static_cast<char>(wc); });

        return std::ranges::equal(wide_string_transformed_to_ascii, right);
      }

      [[nodiscard]] explicit operator bool() const noexcept {
        return m_buffer != nullptr;
      }

      friend std::ostream& operator<<(std::ostream& os, const unicode_string& unicode_str) {
        return os << unicode_str.string();
      }

     private:
      [[nodiscard]] bool contains_non_ascii(const std::wstring_view str) const noexcept {
        return std::ranges::any_of(str, [](wchar_t ch) {
          return ch > 127;  // characters out of ASCII range
        });
      }

      std::uint16_t m_length{0};
      std::uint16_t m_max_length{0};
      pointer_t m_buffer{nullptr};
    };

    struct list_entry {
      list_entry* flink;
      list_entry* blink;
    };

    enum directory_id : std::uint8_t {
      directory_entry_export = 0,           // Export Directory
      directory_entry_import = 1,           // Import Directory
      directory_entry_resource = 2,         // Resource Directory
      directory_entry_exception = 3,        // Exception Directory
      directory_entry_security = 4,         // Security Directory
      directory_entry_basereloc = 5,        // Base Relocation Table
      directory_entry_debug = 6,            // Debug Directory
      directory_entry_copyright = 7,        // (X86 usage)
      directory_entry_architecture = 7,     // Architecture Specific Data
      directory_entry_globalptr = 8,        // RVA of GP
      directory_entry_tls = 9,              // TLS Directory
      directory_entry_load_config = 10,     // Load Configuration Directory
      directory_entry_bound_import = 11,    // Bound Import Directory in headers
      directory_entry_iat = 12,             // Import Address Table
      directory_entry_delay_import = 13,    // Delay Load Import Descriptors
      directory_entry_com_descriptor = 14,  // COM Runtime descriptor
      directory_reserved0 = 15,             // -
    };

    struct data_directory_t {
      std::uint32_t rva;
      std::uint32_t size;
      [[nodiscard]] bool present() const noexcept {
        return size > 0;
      }
    };

    struct raw_data_directory_t {
      uint32_t ptr_raw_data;
      uint32_t size;
      [[nodiscard]] bool present() const noexcept {
        return size > 0;
      }
    };

    struct data_directories_x64_t {
      union {
        struct {
          data_directory_t export_directory;
          data_directory_t import_directory;
          data_directory_t resource_directory;
          data_directory_t exception_directory;
          raw_data_directory_t security_directory;  // File offset instead of RVA!
          data_directory_t basereloc_directory;
          data_directory_t debug_directory;
          data_directory_t architecture_directory;
          data_directory_t globalptr_directory;
          data_directory_t tls_directory;
          data_directory_t load_config_directory;
          data_directory_t bound_import_directory;
          data_directory_t iat_directory;
          data_directory_t delay_import_directory;
          data_directory_t com_descriptor_directory;
          data_directory_t _reserved0;
        };
        data_directory_t entries[num_data_directories];
      };
    };

    struct data_directories_x86_t {
      union {
        struct {
          data_directory_t export_directory;
          data_directory_t import_directory;
          data_directory_t resource_directory;
          data_directory_t exception_directory;
          raw_data_directory_t security_directory;  // File offset instead of RVA!
          data_directory_t basereloc_directory;
          data_directory_t debug_directory;
          data_directory_t copyright_directory;
          data_directory_t globalptr_directory;
          data_directory_t tls_directory;
          data_directory_t load_config_directory;
          data_directory_t bound_import_directory;
          data_directory_t iat_directory;
          data_directory_t delay_import_directory;
          data_directory_t com_descriptor_directory;
          data_directory_t _reserved0;
        };
        data_directory_t entries[num_data_directories];
      };
    };

    struct export_directory_t {
      uint32_t characteristics;
      uint32_t timedate_stamp;
      version_t version;
      uint32_t name;
      uint32_t base;
      uint32_t num_functions;
      uint32_t num_names;
      uint32_t rva_functions;
      uint32_t rva_names;
      uint32_t rva_name_ordinals;

      [[nodiscard]] auto rva_table(address_t base_address) const {
        return base_address.ptr<std::uint32_t>(rva_functions);
      }

      [[nodiscard]] auto ordinal_table(address_t base_address) const {
        return base_address.ptr<std::uint16_t>(rva_name_ordinals);
      }
    };

    struct export_t {
      std::string_view name;
      address_t address;
      std::uint32_t ordinal;
      bool is_forwarded;
    };

    enum class subsystem_id : uint16_t {
      unknown = 0x0000,         // Unknown subsystem.
      native = 0x0001,          // Image doesn't require a subsystem.
      windows_gui = 0x0002,     // Image runs in the Windows GUI subsystem.
      windows_cui = 0x0003,     // Image runs in the Windows character subsystem
      os2_cui = 0x0005,         // image runs in the OS/2 character subsystem.
      posix_cui = 0x0007,       // image runs in the Posix character subsystem.
      native_windows = 0x0008,  // image is a native Win9x driver.
      windows_ce_gui = 0x0009,  // Image runs in the Windows CE subsystem.
      efi_application = 0x000A,
      efi_boot_service_driver = 0x000B,
      efi_runtime_driver = 0x000C,
      efi_rom = 0x000D,
      xbox = 0x000E,
      windows_boot_application = 0x0010,
      xbox_code_catalog = 0x0011,
    };

    struct loader_table_entry {
      list_entry in_load_order_links;
      list_entry in_memory_order_links;
      union {
        list_entry in_initialization_order_links;
        list_entry in_progress_links;
      };
      address_t base_address;
      address_t entry_point;
      std::uint32_t size_image;
      unicode_string path;
      unicode_string name;
      union {
        std::uint8_t flag_group[4];
        std::uint32_t flags;
        struct {
          std::uint32_t packaged_binary : 1;
          std::uint32_t marked_for_removal : 1;
          std::uint32_t image_dll : 1;
          std::uint32_t load_notifications_sent : 1;
          std::uint32_t telemetry_entry_processed : 1;
          std::uint32_t static_import_processed : 1;
          std::uint32_t in_legacy_lists : 1;
          std::uint32_t in_indexes : 1;
          std::uint32_t shim_dll : 1;
          std::uint32_t in_exception_table : 1;
          std::uint32_t reserved_flags_1 : 2;
          std::uint32_t load_in_progress : 1;
          std::uint32_t load_config_processed : 1;
          std::uint32_t entry_point_processed : 1;
          std::uint32_t delay_load_protection_enabled : 1;
          std::uint32_t reserved_flags_3 : 2;
          std::uint32_t skip_thread_calls : 1;
          std::uint32_t process_attach_called : 1;
          std::uint32_t process_attach_failed : 1;
          std::uint32_t cor_validation_deferred : 1;
          std::uint32_t is_cor_image : 1;
          std::uint32_t skip_relocation : 1;
          std::uint32_t is_cor_il_only : 1;
          std::uint32_t is_chpe_image : 1;
          std::uint32_t reserved_flags_5 : 2;
          std::uint32_t redirected : 1;
          std::uint32_t reserved_flags_6 : 2;
          std::uint32_t compatibility_database_processed : 1;
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

    struct user_process_parameters {
      uint8_t reserved1[16];
      std::nullptr_t reserved2[10];
      unicode_string image_path_name;
      unicode_string command_line;
    };

    struct PEB {
      uint8_t reserved1[2];
      uint8_t being_debugged;
      uint8_t reserved2[1];
      std::nullptr_t reserved3[2];
      module_loader_data* ldr_data;
      user_process_parameters* process_parameters;
      std::nullptr_t reserved4[3];
      void* atl_thunk_list_head;
      std::nullptr_t reserved5;
      uint32_t reserved6;
      std::nullptr_t reserved7;
      uint32_t reserved8;
      uint32_t atl_thunk_list_head32;
      void* reserved9[45];
      uint8_t reserved10[96];

      static auto address() noexcept {
#if defined(_M_X64)
        return reinterpret_cast<const PEB*>(__readgsqword(0x60));
#elif defined(_M_IX86)
        return reinterpret_cast<const PEB*>(__readfsdword(0x30));
#else
#error Unsupported platform.
#endif
      }

      static auto loader_data() noexcept {
        return reinterpret_cast<module_loader_data*>(address()->ldr_data);
      }
    };

    struct api_set_namespace {
      uint32_t version;
      uint32_t size;
      uint32_t flags;
      uint32_t count;
      uint32_t entry_offset;
      uint32_t hash_offset;
      uint32_t hash_factor;
    };

    struct api_set_hash_entry {
      uint32_t hash;
      uint32_t index;
    };

    struct api_set_namespace_entry {
      uint32_t flags;
      uint32_t name_offset;
      uint32_t name_length;
      uint32_t hashed_length;
      uint32_t value_offset;
      uint32_t value_count;
    };

    struct api_set_value_entry {
      uint32_t flags;
      uint32_t name_offset;
      uint32_t name_length;
      uint32_t value_offset;
      uint32_t value_length;
    };

    template <std::ranges::view StrTy, typename CharTy = std::ranges::range_value_t<StrTy>>
    constexpr auto remove_api_set_version(const StrTy string) {
      // It is rather a hack because it is impossible to declare a constant
      // string for any of its types. The compiler will be able to
      // substitute these ASCII characters for any type of string.
      constexpr CharTy separator_bytes[] = {'-', 'l', '\0'};
      const StrTy separator{separator_bytes, 2};
      constexpr auto npos = StrTy::npos;

      auto version_pos = string.rfind(separator);
      if (version_pos == npos)
        return StrTy{};

      return string.substr(0, version_pos);
    }

    struct section_header_t {
      section_string_t name;
      union {
        uint32_t physical_address;
        uint32_t virtual_size;
      };
      uint32_t virtual_address;

      uint32_t size_raw_data;
      uint32_t ptr_raw_data;

      uint32_t ptr_relocs;
      uint32_t ptr_line_numbers;
      uint16_t num_relocs;
      uint16_t num_line_numbers;

      uint32_t characteristics_flags;
    };

    struct file_header_t {
      std::uint16_t machine;
      std::uint16_t num_sections;
      std::uint32_t timedate_stamp;
      std::uint32_t ptr_symbols;
      std::uint32_t num_symbols;
      std::uint16_t size_optional_header;
      std::uint16_t characteristics;
    };

    struct optional_header_x64_t {
      // Standard fields.
      uint16_t magic;
      version_t linker_version;
      uint32_t size_code;
      uint32_t size_init_data;
      uint32_t size_uninit_data;
      uint32_t entry_point;
      uint32_t base_of_code;
      uint64_t image_base;
      uint32_t section_alignment;
      uint32_t file_alignment;
      ex_version_t os_version;
      ex_version_t img_version;
      ex_version_t subsystem_version;
      uint32_t win32_version_value;
      uint32_t size_image;
      uint32_t size_headers;
      uint32_t checksum;
      subsystem_id subsystem;
      uint16_t characteristics;
      uint64_t size_stack_reserve;
      uint64_t size_stack_commit;
      uint64_t size_heap_reserve;
      uint64_t size_heap_commit;
      uint32_t ldr_flags;
      uint32_t num_data_directories;
      data_directories_x64_t data_directories;
    };

    struct optional_header_x86_t {
      // Standard fields.
      uint16_t magic;
      version_t linker_version;
      uint32_t size_code;
      uint32_t size_init_data;
      uint32_t size_uninit_data;
      uint32_t entry_point;
      uint32_t base_of_code;
      uint32_t base_of_data;
      uint32_t image_base;
      uint32_t section_alignment;
      uint32_t file_alignment;
      ex_version_t os_version;
      ex_version_t img_version;
      ex_version_t subsystem_version;
      uint32_t win32_version_value;
      uint32_t size_image;
      uint32_t size_headers;
      uint32_t checksum;
      subsystem_id subsystem;
      uint16_t characteristics;
      uint32_t size_stack_reserve;
      uint32_t size_stack_commit;
      uint32_t size_heap_reserve;
      uint32_t size_heap_commit;
      uint32_t ldr_flags;
      uint32_t num_data_directories;
      data_directories_x86_t data_directories;

      inline bool has_directory(const data_directory_t* dir) const {
        return &data_directories.entries[num_data_directories] < dir && dir->present();
      }

      inline bool has_directory(directory_id id) const {
        return has_directory(&data_directories.entries[id]);
      }
    };

    using optional_header_t =
        std::conditional_t<is_x64, optional_header_x64_t, optional_header_x86_t>;

    struct nt_headers_t {
      uint32_t signature;
      file_header_t file_header;
      optional_header_t optional_header;

      // Section getters
      inline section_header_t* get_sections() {
        return (section_header_t*)((uint8_t*)&optional_header + file_header.size_optional_header);
      }
      inline section_header_t* get_section(size_t n) {
        return n >= file_header.num_sections ? nullptr : get_sections() + n;
      }
      inline const section_header_t* get_sections() const {
        return const_cast<nt_headers_t*>(this)->get_sections();
      }
      inline const section_header_t* get_section(size_t n) const {
        return const_cast<nt_headers_t*>(this)->get_section(n);
      }

      // Section iterator
      template <typename T>
      struct proxy {
        T* base;
        uint16_t count;
        T* begin() const {
          return base;
        }
        T* end() const {
          return base + count;
        }
      };
      inline proxy<section_header_t> sections() {
        return {get_sections(), file_header.num_sections};
      }
      inline proxy<const section_header_t> sections() const {
        return {get_sections(), file_header.num_sections};
      }
    };

    struct dos_header_t {
      uint16_t e_magic;
      uint16_t e_cblp;
      uint16_t e_cp;
      uint16_t e_crlc;
      uint16_t e_cparhdr;
      uint16_t e_minalloc;
      uint16_t e_maxalloc;
      uint16_t e_ss;
      uint16_t e_sp;
      uint16_t e_csum;
      uint16_t e_ip;
      uint16_t e_cs;
      uint16_t e_lfarlc;
      uint16_t e_ovno;
      uint16_t e_res[4];
      uint16_t e_oemid;
      uint16_t e_oeminfo;
      uint16_t e_res2[10];
      uint32_t e_lfanew;

      inline file_header_t* get_file_header() {
        return &get_nt_headers()->file_header;
      }
      inline const file_header_t* get_file_header() const {
        return &get_nt_headers()->file_header;
      }
      inline nt_headers_t* get_nt_headers() {
        return (nt_headers_t*)((uint8_t*)this + e_lfanew);
      }
      inline const nt_headers_t* get_nt_headers() const {
        return const_cast<dos_header_t*>(this)->get_nt_headers();
      }
    };

    struct image_t {
      dos_header_t dos_header;

      // Basic getters.
      inline dos_header_t* get_dos_headers() {
        return &dos_header;
      }
      inline const dos_header_t* get_dos_headers() const {
        return &dos_header;
      }
      inline file_header_t* get_file_header() {
        return dos_header.get_file_header();
      }
      inline const file_header_t* get_file_header() const {
        return dos_header.get_file_header();
      }
      inline nt_headers_t* get_nt_headers() {
        return dos_header.get_nt_headers();
      }
      inline const nt_headers_t* get_nt_headers() const {
        return dos_header.get_nt_headers();
      }
      inline optional_header_t* get_optional_header() {
        return &get_nt_headers()->optional_header;
      }
      inline const optional_header_t* get_optional_header() const {
        return &get_nt_headers()->optional_header;
      }

      inline data_directory_t* get_directory(directory_id id) {
        auto nt_hdrs = get_nt_headers();
        if (nt_hdrs->optional_header.num_data_directories <= id)
          return nullptr;
        data_directory_t* dir = &nt_hdrs->optional_header.data_directories.entries[id];
        return dir->present() ? dir : nullptr;
      }

      inline const data_directory_t* get_directory(directory_id id) const {
        return const_cast<image_t*>(this)->get_directory(id);
      }

      template <typename T = uint8_t>
      inline T* rva_to_ptr(uint32_t rva, size_t length = 1) {
        // Find the section, try mapping to header if none found.
        auto scn = rva_to_section(rva);
        if (!scn) {
          uint32_t rva_hdr_end = get_nt_headers()->optional_header.size_headers;
          if (rva < rva_hdr_end && (rva + length) <= rva_hdr_end)
            return (T*)((uint8_t*)&dos_header + rva);
          return nullptr;
        }

        // Apply the boundary check.
        size_t offset = rva - scn->virtual_address;
        if ((offset + length) > scn->size_raw_data)
          return nullptr;

        // Return the final pointer.
        return (T*)((uint8_t*)&dos_header + scn->ptr_raw_data + offset);
      }

      inline section_header_t* rva_to_section(uint32_t rva) {
        auto nt_hdrs = get_nt_headers();
        for (size_t i = 0; i != nt_hdrs->file_header.num_sections; i++) {
          auto section = nt_hdrs->get_section(i);
          if (section->virtual_address <= rva &&
              rva < (section->virtual_address + section->virtual_size))
            return section;
        }
        return nullptr;
      }

      template <typename T = uint8_t>
      inline const T* rva_to_ptr(uint32_t rva, size_t length = 1) const {
        return const_cast<image_t*>(this)->template rva_to_ptr<const T>(rva, length);
      }
      inline uint32_t rva_to_fo(uint32_t rva, size_t length = 1) const {
        return ptr_to_raw(rva_to_ptr(rva, length));
      }
      inline uint32_t ptr_to_raw(const void* ptr) const {
        return ptr ? uint32_t(uintptr_t(ptr) - uintptr_t(&dos_header)) : img_npos;
      }
    };

    inline auto image_from_base(address_t base) {
      return base.ptr<image_t>();
    }

    inline auto image_from_base(loader_table_entry* module) {
      return image_from_base(module->base_address);
    }

    template <typename T, typename FieldT>
    constexpr T* containing_record(FieldT* address, FieldT T::* field) {
      auto offset = reinterpret_cast<std::uintptr_t>(&(reinterpret_cast<T*>(0)->*field));
      return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(address) - offset);
    }

    struct kernel_system_time {
      uint32_t low_part;
      int32_t high1_time;
      int32_t high2_time;
    };

    enum nt_product_type { win_nt = 1, lan_man_nt = 2, server = 3 };

    enum alternative_arch_type { standart_design, nec98x86, end_alternatives };

    struct xstate_feature {
      uint32_t offset;
      uint32_t size;
    };

    struct xstate_configuration {
      // Mask of all enabled features
      uint64_t enabled_features;
      // Mask of volatile enabled features
      uint64_t enabled_volatile_features;
      // Total size of the save area for user states
      uint32_t size;
      // Control Flags
      union {
        uint32_t control_flags;
        struct {
          uint32_t optimized_save : 1;
          uint32_t compaction_enabled : 1;
          uint32_t extended_feature_disable : 1;
        };
      };
      // List of features
      xstate_feature features[64];
      // Mask of all supervisor features
      uint64_t enabled_supervisor_features;
      // Mask of features that require start address to be 64 byte aligned
      uint64_t aligned_features;
      // Total size of the save area for user and supervisor states
      uint32_t all_features_size;
      // List which holds size of each user and supervisor state supported by CPU
      uint32_t all_features[64];
      // Mask of all supervisor features that are exposed to user-mode
      uint64_t enabled_user_visible_supervisor_features;
      // Mask of features that can be disabled via XFD
      uint64_t extended_feature_disable_features;
      // Total size of the save area for non-large user and supervisor states
      uint32_t all_non_large_feature_size;
      uint32_t spare;
    };

    union win32_large_integer {
      struct {
        uint32_t low_part;
        int32_t high_part;
      };
      struct {
        uint32_t low_part;
        int32_t high_part;
      } u;
      uint64_t quad_part;
    };

    struct kernel_user_shared_data {
      uint32_t tick_count_low_deprecated;
      uint32_t tick_count_multiplier;
      kernel_system_time interrupt_time;
      kernel_system_time system_time;
      kernel_system_time time_zone_bias;
      uint16_t image_number_low;
      uint16_t image_number_high;
      wchar_t nt_system_root[260];
      uint32_t max_stack_trace_depth;
      uint32_t crypto_exponent;
      uint32_t time_zone_id;
      uint32_t large_page_minimum;
      uint32_t ait_sampling_value;
      uint32_t app_compat_flag;
      uint64_t random_seed_version;
      uint32_t global_validation_runlevel;
      int32_t time_zone_bias_stamp;
      uint32_t nt_build_number;
      nt_product_type nt_product_type;
      bool product_type_is_valid;
      bool reserved0[1];
      uint16_t native_processor_architecture;
      uint32_t nt_major_version;
      uint32_t nt_minor_version;
      bool processor_features[64];
      uint32_t reserved1;
      uint32_t reserved3;
      uint32_t time_slip;
      alternative_arch_type alternative_arch;
      uint32_t boot_id;
      win32_large_integer system_expiration_date;
      uint32_t suite_mask;
      bool kernel_debugger_enabled;
      union {
        uint8_t mitigation_policies;
        struct {
          uint8_t nx_support_policy : 2;
          uint8_t seh_validation_policy : 2;
          uint8_t cur_dir_devices_skipped_for_dlls : 2;
          uint8_t reserved : 2;
        };
      };
      uint16_t cycles_per_yield;
      uint32_t active_console_id;
      uint32_t dismount_count;
      uint32_t com_plus_package;
      uint32_t last_system_rit_event_tick_count;
      uint32_t number_of_physical_pages;
      bool safe_boot_mode;
      union {
        uint8_t virtualization_flags;
        struct {
          uint8_t arch_started_in_el2 : 1;
          uint8_t qc_sl_is_supported : 1;
        };
      };
      uint8_t reserved12[2];
      union {
        uint32_t shared_data_flags;
        struct {
          uint32_t dbg_error_port_present : 1;
          uint32_t dbg_elevation_enabled : 1;
          uint32_t dbg_virt_enabled : 1;
          uint32_t dbg_installer_detect_enabled : 1;
          uint32_t dbg_lkg_enabled : 1;
          uint32_t dbg_dyn_processor_enabled : 1;
          uint32_t dbg_console_broker_enabled : 1;
          uint32_t dbg_secure_boot_enabled : 1;
          uint32_t dbg_multi_session_sku : 1;
          uint32_t dbg_multi_users_in_session_sku : 1;
          uint32_t dbg_state_separation_enabled : 1;
          uint32_t spare_bits : 21;
        };
      };
      uint32_t data_flags_pad[1];
      uint64_t test_ret_instruction;
      int64_t qpc_frequency;
      uint32_t system_call;
      uint32_t reserved2;
      uint64_t full_number_of_physical_pages;
      uint64_t system_call_pad[1];
      union {
        kernel_system_time tick_count;
        uint64_t tick_count_quad;
        struct {
          uint32_t reserved_tick_count_overlay[3];
          uint32_t tick_count_pad[1];
        };
      };
      uint32_t cookie;
      uint32_t cookie_pad[1];
      int64_t console_session_foreground_process_id;
      uint64_t time_update_lock;
      uint64_t baseline_system_time_qpc;
      uint64_t baseline_interrupt_time_qpc;
      uint64_t qpc_system_time_increment;
      uint64_t qpc_interrupt_time_increment;
      uint8_t qpc_system_time_increment_shift;
      uint8_t qpc_interrupt_time_increment_shift;
      uint16_t unparked_processor_count;
      uint32_t enclave_feature_mask[4];
      uint32_t telemetry_coverage_round;
      uint16_t user_mode_global_logger[16];
      uint32_t image_file_execution_options;
      uint32_t lang_generation_count;
      uint64_t reserved4;
      uint64_t interrupt_time_bias;
      uint64_t qpc_bias;
      uint32_t active_processor_count;
      uint8_t active_group_count;
      uint8_t reserved9;
      union {
        uint16_t qpc_data;
        struct {
          uint8_t qpc_bypass_enabled;
          uint8_t qpc_reserved;
        };
      };
      win32_large_integer time_zone_bias_effective_start;
      win32_large_integer time_zone_bias_effective_end;
      xstate_configuration xstate;
      kernel_system_time feature_configuration_change_stamp;
      uint32_t spare;
      uint64_t user_pointer_auth_mask;
      xstate_configuration xstate_arm64;
      uint32_t reserved10[210];
    };

  }  // namespace win

  namespace detail {

    template <typename Ty>
    struct type_hash {
      auto operator()(const Ty& instance) const noexcept {
        return std::hash<typename Ty::underlying_t>()(instance.get());
      }
    };

    template <typename Ty>
    struct type_format : std::formatter<typename Ty::underlying_t> {
      auto format(const Ty& value, std::format_context& ctx) const {
        return std::formatter<typename Ty::underlying_t>::format(value.get(), ctx);
      }
    };

    template <std::size_t N>
    struct fixed_string {
      char value[N];

      consteval fixed_string(const char (&str)[N]) {
        for (std::size_t i = 0; i < N; ++i) {
          value[i] = str[i];
        }
      }

      constexpr std::string_view view() const {
        // drop the trailing '\0' when exposing as string_view
        return std::string_view{value, N - 1};
      }
    };

    template <typename Ret, typename... Args>
    class stack_function;

    template <typename Ret, typename... Args>
    class stack_function<Ret(Args...)> {
     public:
      using function_ptr_t = Ret (*)(void*, Args&&...);
      using destructor_ptr_t = void (*)(void*);

      stack_function() = default;

      template <typename F, typename DecayedF = std::decay_t<F>>
        requires(std::is_invocable_r_v<Ret, F, Args...>)
      stack_function(F&& func) {
        static_assert(sizeof(DecayedF) <= sizeof(m_storage), "Function object too large");

        // Here we use "placement new" for SBO, which does
        // not result in any additional memory allocation
        // https://en.cppreference.com/w/cpp/language/new#Placement_new
        new (&m_storage) DecayedF(std::forward<F>(func));

        m_invoker = [](void* ptr, Args&&... args) -> Ret {
          return (*reinterpret_cast<DecayedF*>(ptr))(std::forward<Args>(args)...);
        };

        if constexpr (std::is_destructible_v<DecayedF>) {
          m_destroyer = [](void* ptr) {
            std::destroy_at(reinterpret_cast<std::decay_t<F>*>(ptr));
          };
        }
      }

      ~stack_function() {
        if (m_destroyer) {
          m_destroyer(&m_storage);
        }
      }

      void swap(stack_function& other) noexcept {
        if (m_destroyer)
          m_destroyer(&m_storage);
        if (other.m_destroyer)
          other.m_destroyer(&other.m_storage);

        std::swap(m_storage, other.m_storage);
        std::swap(m_invoker, other.m_invoker);
        std::swap(m_destroyer, other.m_destroyer);
      }

      Ret operator()(Args... args) {
#if SHADOW_HAS_EXCEPTIONS
        if (!m_invoker)
          throw std::bad_function_call();
#endif
        return m_invoker(&m_storage, std::forward<Args>(args)...);
      }

     private:
      alignas(void*) std::byte m_storage[32];
      function_ptr_t m_invoker{nullptr};
      destructor_ptr_t m_destroyer{nullptr};
    };

#ifdef SHADOWSYSCALLS_SEED
    constexpr uint64_t library_seed = SHADOWSYSCALLS_SEED;
#else
    constexpr uint64_t library_seed = 0x8953484829149489ull;
#endif

    // basic_hash class provides compile-time and runtime hash
    // computation. Uses FNV-1a hashing algorithm.
    // Case-insensitive by default.
    template <std::integral ValTy>
    class basic_hash {
     public:
      using underlying_t = ValTy;
      constexpr static bool case_sensitive = false;
      constexpr static ValTy FNV_prime = (sizeof(ValTy) == 4) ? 16777619u : 1099511628211ull;
      constexpr static ValTy FNV_offset_basis =
          (sizeof(ValTy) == 4) ? 2166136261u : 14695981039346656037ull;

     public:
      constexpr basic_hash(ValTy hash) : m_value(hash) {}
      constexpr basic_hash() = default;
      constexpr ~basic_hash() = default;

      // The compile-time constructor, consteval gives an
      // absolute guarantee that the hash of the string
      // will be computed at compile time, so the output
      // of any string will be a number.
      template <typename CharT, std::size_t N>
      consteval basic_hash(const CharT (&string)[N]) {
        constexpr auto string_length = N - 1;
        for (auto i = 0; i < string_length; i++)
          m_value = fnv1a_append_bytes<>(m_value, string[i]);
      }

      consteval basic_hash(const char* string, std::size_t len) {
        for (auto i = 0; i < len; i++)
          m_value = fnv1a_append_bytes<>(m_value, string[i]);
      }

      // Method for calculating hash at runtime. Accepts
      // any object with range properties.
      template <concepts::hashable Ty>
      [[nodiscard]] ValTy operator()(const Ty& object) {
        ValTy local_value = m_value;
        for (auto i = 0; i < object.size(); i++)
          local_value = fnv1a_append_bytes<>(local_value, object[i]);
        return local_value;
      }

      // \return Hash-value copy as an integral
      [[nodiscard]] constexpr ValTy get() const {
        return m_value;
      }

      [[nodiscard]] constexpr explicit operator ValTy() const {
        return m_value;
      }

      constexpr auto operator<=>(const basic_hash&) const = default;

      friend std::ostream& operator<<(std::ostream& os, const basic_hash& hash) {
        return os << hash.m_value;
      }

     private:
      template <typename CharTy>
      [[nodiscard]] constexpr ValTy fnv1a_append_bytes(ValTy value,
                                                       const CharTy byte) const noexcept {
        const auto lowercase_byte = case_sensitive ? byte : to_lower(byte);
        value ^= static_cast<ValTy>(lowercase_byte);
        value *= FNV_prime;
        return value;
      }

      template <typename CharTy>
      [[nodiscard]] constexpr CharTy to_lower(CharTy c) const {
        return ((c >= 'A' && c <= 'Z') ? (c + 32) : c);
      }

     private:
      ValTy m_value{FNV_offset_basis + static_cast<ValTy>(library_seed)};
    };

    using hash32_t = detail::basic_hash<uint32_t>;
    using hash64_t = detail::basic_hash<uint64_t>;

    class memory_size {
      constexpr static auto conversion_value = 1000.0;

     public:
      template <std::integral Ty>
      explicit memory_size(Ty bytes) noexcept : m_bytes(bytes) {}

      [[nodiscard]] auto as_bytes() const noexcept {
        return m_bytes;
      }

      [[nodiscard]] auto as_kilobytes() const noexcept {
        return static_cast<std::double_t>(m_bytes) / conversion_value;
      }

      [[nodiscard]] auto as_megabytes() const noexcept {
        return static_cast<std::double_t>(m_bytes) / (std::pow(conversion_value, 2));
      }

      [[nodiscard]] auto as_gigabytes() const noexcept {
        return static_cast<std::double_t>(m_bytes) / (std::pow(conversion_value, 3));
      }

      // Use implicit conversion
      operator std::size_t() const noexcept {
        return m_bytes;
      }

     private:
      std::size_t m_bytes;
    };

    class cpu_info {
      class caches_info;

     public:
      cpu_info() noexcept {
        parse_cpu_fields();
      }

      [[nodiscard]] bool is_intel() const noexcept {
        return m_is_intel;
      }
      [[nodiscard]] bool is_amd() const noexcept {
        return m_is_amd;
      }

      // \return caches returns CPU caches information
      // (only works for Intel so far)
      [[nodiscard]] std::optional<caches_info> caches() const noexcept {
        if (is_intel()) {
          return caches_info{};
        } else {
          // Not really sure about processors other than Intel.
          // Any good PR with a solution is greatly appreciated
          return std::nullopt;
        }
      }

      // \return returns processor vendor name
      [[nodiscard]] std::string vendor() const noexcept {
        return m_vendor;
      }

      // \return returns cpu full name
      [[nodiscard]] std::string brand() const noexcept {
        return m_brand;
      }

#define SUPPORTS(instruction_set_name, condition)                       \
  [[nodiscard]] bool supports_##instruction_set_name() const noexcept { \
    return condition;                                                   \
  }

      // \return returns true if SSE (Streaming SIMD Extensions) is supported
      SUPPORTS(sse, m_standard_features_edx[25]);
      // \return returns true if SSE2 is supported by the CPU
      SUPPORTS(sse2, m_standard_features_edx[26]);
      // \return returns true if SSE3 is supported by the CPU
      SUPPORTS(sse3, m_standard_features_ecx[0]);
      // \return returns true if SSSE3 is supported by the CPU
      SUPPORTS(ssse3, m_standard_features_ecx[9]);
      // \return returns true if SSE4.1 is supported by the CPU
      SUPPORTS(sse4_1, m_standard_features_ecx[19]);
      // \return returns true if SSE4.2 is supported by the CPU
      SUPPORTS(sse4_2, m_standard_features_ecx[20]);

      // \return returns true if AVX (Advanced Vector Extensions) is supported
      SUPPORTS(avx, m_standard_features_ecx[28]);
      // \return returns true if AVX2 is supported by the CPU
      SUPPORTS(avx2, m_extended_features_ebx[5]);
      // \return returns true if AVX-512 Foundation is supported
      SUPPORTS(avx512f, m_extended_features_ebx[16]);
      // \return returns true if AVX-512 Prefetch is supported
      SUPPORTS(avx512pf, m_extended_features_ebx[26]);
      // \return returns true if AVX-512 Exponential and Reciprocal is supported
      SUPPORTS(avx512er, m_extended_features_ebx[27]);
      // \return returns true if AVX-512 Conflict Detection is supported
      SUPPORTS(avx512cd, m_extended_features_ebx[28]);

      // AMD-Specific Extensions
      // \return returns true if SSE4a is supported on AMD CPUs
      SUPPORTS(sse4a, m_is_amd&& m_amd_extended_features_ecx[6]);
      // \return returns true if LAHF/SAHF is supported in 64-bit mode
      SUPPORTS(lahf, m_amd_extended_features_ecx[0]);
      // \return returns true if ABM (Advanced Bit Manipulation) is supported on AMD CPUs
      SUPPORTS(abm, m_is_amd&& m_amd_extended_features_ecx[5]);
      // \return returns true if XOP (Extended Operations) is supported on AMD CPUs
      SUPPORTS(xop, m_is_amd&& m_amd_extended_features_ecx[11]);
      // \return returns true if TBM (Trailing Bit Manipulation) is supported on AMD CPUs
      SUPPORTS(tbm, m_is_amd&& m_amd_extended_features_ecx[21]);
      // \return returns true if MMX extensions are supported on AMD CPUs
      SUPPORTS(mmxext, m_is_amd&& m_amd_extended_features_edx[22]);

      // Other Instruction Set Extensions
      // \return returns true if PCLMULQDQ (Carry-Less Multiplication) is supported
      SUPPORTS(pclmulqdq, m_standard_features_ecx[1]);
      // \return returns true if MONITOR/MWAIT instructions are supported
      SUPPORTS(monitor, m_standard_features_ecx[3]);
      // \return returns true if FMA (Fused Multiply-Add) is supported
      SUPPORTS(fma, m_standard_features_ecx[12]);
      // \return returns true if CMPXCHG16B is supported by the CPU
      SUPPORTS(cmpxchg16b, m_standard_features_ecx[13]);
      // \return returns true if MOVBE (Move with Byte Swap) is supported
      SUPPORTS(movbe, m_standard_features_ecx[22]);
      // \return returns true if POPCNT (Population Count) instruction is supported
      SUPPORTS(popcnt, m_standard_features_ecx[23]);
      // \return returns true if AES-NI (Advanced Encryption Standard) is supported
      SUPPORTS(aes, m_standard_features_ecx[25]);
      // \return returns true if XSAVE/XRSTOR instructions are supported
      SUPPORTS(xsave, m_standard_features_ecx[26]);
      // \return returns true if OSXSAVE (Operating System XSave) is supported
      SUPPORTS(osxsave, m_standard_features_ecx[27]);
      // \return returns true if RDRAND (Hardware Random Number Generator) is supported
      SUPPORTS(rdrand, m_standard_features_ecx[30]);
      // \return returns true if F16C (16-bit Floating-Point Conversion) is supported
      SUPPORTS(f16c, m_standard_features_ecx[29]);
      // \return returns true if MSR (Model-Specific Registers) are supported
      SUPPORTS(msr, m_standard_features_edx[5]);
      // \return returns true if CMPXCHG8 instruction is supported
      SUPPORTS(cx8, m_standard_features_edx[8]);
      // \return returns true if SYSENTER/SYSEXIT instructions are supported
      SUPPORTS(sep, m_standard_features_edx[11]);
      // \return returns true if CMOV (Conditional Move) is supported
      SUPPORTS(cmov, m_standard_features_edx[15]);
      // \return returns true if CLFLUSH (Cache Line Flush) instruction is supported
      SUPPORTS(clflush, m_standard_features_edx[19]);
      // \return returns true if MMX (MultiMedia Extensions) is supported
      SUPPORTS(mmx, m_standard_features_edx[23]);
      // \return returns true if FXSAVE/FXRSTOR instructions are supported
      SUPPORTS(fxsr, m_standard_features_edx[24]);
      // \return returns true if FSGSBASE instructions are supported
      SUPPORTS(fsgsbase, m_extended_features_ebx[0]);
      // \return returns true if BMI1 (Bit Manipulation Instructions Set 1) is supported
      SUPPORTS(bmi1, m_extended_features_ebx[3]);
      // \return returns true if BMI2 (Bit Manipulation Instructions Set 2) is supported
      SUPPORTS(bmi2, m_extended_features_ebx[8]);
      // \return returns true if HLE (Hardware Lock Elision) is supported on Intel CPUs
      SUPPORTS(hle, m_is_intel&& m_extended_features_ebx[4]);
      // \return returns true if Enhanced REP MOVSB/STOSB is supported
      SUPPORTS(erms, m_extended_features_ebx[9]);
      // \return returns true if INVPCID (Invalidate Process-Context Identifier) is supported
      SUPPORTS(invpcid, m_extended_features_ebx[10]);
      // \return returns true if RTM (Restricted Transactional Memory) is supported on Intel CPUs
      SUPPORTS(rtm, m_is_intel&& m_extended_features_ebx[11]);
      // \return returns true if RDSEED (Random Seed) instruction is supported
      SUPPORTS(rdseed, m_extended_features_ebx[18]);
      // \return returns true if ADX (Multi-Precision Add-Carry Instruction Extensions) is supported
      SUPPORTS(adx, m_extended_features_ebx[19]);
      // \return returns true if SHA (Secure Hash Algorithm) instructions are supported
      SUPPORTS(sha, m_extended_features_ebx[29]);
      // \return returns true if PREFETCHWT1 instruction is supported
      SUPPORTS(prefetchwt1, m_extended_features_ecx[0]);
      // \return returns true if SYSCALL/SYSRET instructions are supported on Intel CPUs
      SUPPORTS(syscall, m_is_intel&& m_amd_extended_features_edx[11]);
      // \return returns true if LZCNT (Leading Zero Count) is supported on Intel CPUs
      SUPPORTS(lzcnt, m_is_intel&& m_amd_extended_features_ecx[5]);
      // \return returns true if RDTSCP (Read Time-Stamp Counter) instruction is supported on Intel CPUs
      SUPPORTS(rdtscp, m_is_intel&& m_amd_extended_features_edx[27]);

#undef SUPPORTS

     private:
      constexpr static auto cpuid_base = 0x80000000;

      class caches_info {
       public:
        caches_info() noexcept {
          parse_cache_info();
        }

        [[nodiscard]] auto l1_size() const noexcept {
          return memory_size{m_cache_sizes[0]};
        }

        [[nodiscard]] auto l2_size() const noexcept {
          return memory_size{m_cache_sizes[1]};
        }

        [[nodiscard]] auto l3_size() const noexcept {
          return memory_size{m_cache_sizes[2]};
        }

        [[nodiscard]] auto total_size() const noexcept {
          return memory_size{l1_size() + l2_size() + l3_size()};
        }

       private:
        void parse_cache_info() noexcept {
          std::array<std::int32_t, 4> cpu_info{};

          // CPUID for cache hierarchy (EAX=4)
          for (int i = 0;; ++i) {
            __cpuidex(cpu_info.data(), 4, i);

            // Check cache type (bits [3:0] of EAX) - 0 means no more caches
            std::int32_t cache_type = cpu_info[0] & 0xF;
            if (cache_type == 0)
              break;  // No more caches

            // Extract cache level (bits [7:5] of EAX)
            std::int32_t cache_level = (cpu_info[0] >> 5) & 0x7;
            std::int32_t cache_size =
                ((cpu_info[1] >> 22) + 1) *    // Number of sets
                ((cpu_info[1] & 0xFFF) + 1) *  // Line size (in bytes)
                ((cpu_info[2] & 0x3FF) + 1) *  // Associativity (ways of set associativity)
                (cpu_info[3] + 1);             // Number of partitions

            // Adjust cache_level (1-3) to array index (0-2)
            m_cache_sizes[cache_level - 1] = cache_size;
          }
        }

        std::array<std::int32_t, 3> m_cache_sizes;
      };

      // https://en.wikipedia.org/wiki/CPUID
      void parse_cpu_fields() noexcept {
        std::array<int, 4> cpu_info{};

        // Get highest standard CPUID function ID
        __cpuid(cpu_info.data(), 0);
        m_max_standard_id = cpu_info[0];

        // Query and store information for all standard CPUID functions
        for (std::int32_t i = 0; i <= m_max_standard_id; ++i) {
          __cpuidex(cpu_info.data(), i, 0);
          m_standard_data.push_back(cpu_info);
        }

        m_vendor = extract_cpu_vendor();

        // Read standard feature flags from function
        // 0x00000001 (ECX and EDX registers)
        if (m_max_standard_id >= 1) {
          m_standard_features_ecx = m_standard_data[1][2];  // ECX features
          m_standard_features_edx = m_standard_data[1][3];  // EDX features
        }

        // Read extended feature flags from function
        // 0x00000007 (EBX and ECX registers)
        if (m_max_standard_id >= 7) {
          m_extended_features_ebx = m_standard_data[7][1];  // EBX features
          m_extended_features_ecx = m_standard_data[7][2];  // ECX features
        }

        // To determine the highest supported extended CPUID
        // function, call CPUID with EAX = 0x80000000
        __cpuid(cpu_info.data(), cpuid_base);
        m_max_extended_id = cpu_info[0];

        // Gather information for all extended CPUID
        // functions starting from 0x80000000
        for (std::int32_t i = cpuid_base; i <= m_max_extended_id; ++i) {
          __cpuidex(cpu_info.data(), i, 0);
          m_extended_data.push_back(cpu_info);
        }

        // Read extended feature flags (ECX and EDX)
        // from function 0x80000001
        if (m_max_extended_id >= cpuid_base + 1) {
          m_amd_extended_features_ecx = m_extended_data[1][2];  // ECX features
          m_amd_extended_features_edx = m_extended_data[1][3];  // EDX features
        }

        // Extract the processor brand string from
        // functions 0x80000002 to 0x80000004
        if (m_max_extended_id >= cpuid_base + 4) {
          m_brand = extract_cpu_brand();
        }
      }

      std::string extract_cpu_vendor() noexcept {
        std::array<char, 12> vendor_bytes{};
        std::array<int, 3> vendor_ids = {m_standard_data[0][1], m_standard_data[0][3],
                                         m_standard_data[0][2]};
        memcpy(vendor_bytes.data(), vendor_ids.data(), sizeof(vendor_ids));

        const std::string vendor_str(vendor_bytes.data(), vendor_bytes.size());
        const auto hashed_str = hash64_t{}(vendor_str);

        // Intel || Intel (rare)
        if (hashed_str == hash64_t("GenuineIntel") || hashed_str == hash64_t("GenuineIotel")) {
          m_is_intel = true;
        }
        // AMD || Early samples of AMD K5 processor
        else if (hashed_str == hash64_t("AuthenticAMD") || hashed_str == hash64_t("AMD ISBETTER")) {
          m_is_amd = true;
        }

        return vendor_str;
      }

      std::string extract_cpu_brand() const noexcept {
        std::array<char, 48> vendor_bytes{};
        memcpy(vendor_bytes.data(), m_extended_data[2].data(), sizeof(m_extended_data[2]));
        memcpy(vendor_bytes.data() + 16, m_extended_data[3].data(), sizeof(m_extended_data[3]));
        memcpy(vendor_bytes.data() + 32, m_extended_data[4].data(), sizeof(m_extended_data[4]));
        return std::string(vendor_bytes.begin(), vendor_bytes.end());
      }

      std::int32_t m_max_standard_id{0};
      std::int32_t m_max_extended_id{0};
      std::string m_vendor;
      std::string m_brand;
      bool m_is_intel{false};
      bool m_is_amd{false};
      std::bitset<32> m_standard_features_ecx;
      std::bitset<32> m_standard_features_edx;
      std::bitset<32> m_extended_features_ebx;
      std::bitset<32> m_extended_features_ecx;
      std::bitset<32> m_amd_extended_features_ecx;
      std::bitset<32> m_amd_extended_features_edx;
      std::vector<std::array<int, 4>> m_standard_data;
      std::vector<std::array<int, 4>> m_extended_data;
    };

    // \note: Some useful benchmarks to understand the
    // difference in the bytewise vs collection rate
    // using SSE intrinsics, benched using 2MB span:
    // [MSVC]
    // BM_SumBytesBasic     792456 ns       802176 ns
    // BM_SumBytesSSE        85171 ns        85794 ns
    // [CLANG]
    // BM_SumBytesBasic     381638 ns       383650 ns
    // BM_SumBytesSSE        98902 ns        97656 ns

    template <std::integral Ty = std::size_t>
    class memory_checksum {
      using vector128_t = __m128i;

     public:
      explicit memory_checksum(const std::span<const uint8_t> data) noexcept {
        const auto size = data.size();
        auto sum = _mm_setzero_si128();
        std::size_t pos = 0;

        // The main feature of vectorized byte collection is
        // that we do not iterate each byte separately, but
        // load 16 bytes in one iteration, respectively, the
        // number of iterations is reduced by 16 times.

        for (; pos + 16 <= size; pos += 16)
          process_block(data, pos, sum);

        // Just sum up all 16-bit words from the "sum"
        m_result = sum_16bit_words(sum);

        // If the sum of bytes is not a multiple of 16, there
        // will be a "tail" of remaining bytes, collect them.
        m_result += append_tail(data, pos) * (std::numeric_limits<Ty>::max)();
      }

      [[nodiscard]] Ty result() const noexcept {
        return m_result;
      }

     private:
      void process_block(const std::span<const uint8_t> data, std::size_t pos,
                         vector128_t& sum) const {
        // Load 16 bytes in one-go
        auto block = _mm_loadu_si128(reinterpret_cast<const vector128_t*>(&data[pos]));

        // We will not use '_mm_cvtepi8_epi16' in order
        // not to switch from SSE2 to SSE 4.1
        auto low_eight_bytes = _mm_unpacklo_epi8(block, _mm_setzero_si128());
        auto high_eight_bytes = _mm_unpackhi_epi8(block, _mm_setzero_si128());

        sum = _mm_add_epi16(sum, low_eight_bytes);
        sum = _mm_add_epi16(sum, high_eight_bytes);
      }

      Ty sum_16bit_words(const vector128_t& sum) const {
        alignas(16) int16_t temp[8];
        _mm_storeu_si128(reinterpret_cast<vector128_t*>(temp), sum);

        return std::accumulate(std::begin(temp), std::end(temp), Ty{0});
      }

      Ty append_tail(const std::span<const uint8_t> data, std::size_t pos) const {
        return std::reduce(data.begin() + pos, data.end(), Ty{}, [](Ty acc, char byte) {
          return acc + static_cast<unsigned char>(byte);
        });
      }

      Ty m_result{0};
    };

    class api_set_contract {
      struct api_set_version {
        uint16_t major;
        uint16_t minor;
        uint16_t micro;
      };

      template <std::integral Ty>
      Ty convert_version_symbols_to_integral(std::wstring_view symbols) const noexcept {
        using namespace std::ranges;

        auto ascii_view = symbols | views::filter([](wchar_t c) { return c < 128; }) |
                          views::transform([](wchar_t c) { return static_cast<char>(c); }) |
                          views::take(33);

        auto count = std::ranges::distance(ascii_view);
        if (count > 32)
          return static_cast<Ty>(0);

        std::array<char, 32> buf{};
        std::ranges::copy(ascii_view, buf.begin());

        Ty value = 0;
        auto [ptr, ec] = std::from_chars(buf.data(), buf.data() + count, value);
        return ec == std::errc() ? static_cast<Ty>(value) : static_cast<Ty>(0);
      }

     public:
      constexpr api_set_contract() = default;
      explicit api_set_contract(std::wstring_view name) noexcept : m_name(name) {}

      [[nodiscard]] auto name() const noexcept {
        return m_name;
      }
      [[nodiscard]] auto clean_name() const noexcept {
        return win::remove_api_set_version(m_name);
      }

      [[nodiscard]] auto version() const noexcept {
        constexpr auto separator = '-';
        constexpr auto npos = std::wstring_view::npos;

        // Contract name should have the following format: "-l<major>-<minor>-<micro>"
        auto pos_micro = m_name.find_last_of(separator);
        if (pos_micro == npos)
          return api_set_version{0, 0, 0};

        auto pos_minor = m_name.substr(0, pos_micro).find_last_of(separator);
        if (pos_minor == npos)
          return api_set_version{0, 0, 0};

        auto pos_major = m_name.substr(0, pos_minor).find_last_of(separator);
        if (pos_major == npos)
          return api_set_version{0, 0, 0};

        auto micro_str = m_name.substr(pos_micro + 1);
        auto minor_str = m_name.substr(pos_minor + 1, pos_micro - pos_minor - 1);
        auto major_str = m_name.substr(pos_major + 1, pos_minor - pos_major - 1);

        if (!major_str.empty() && (major_str.front() == L'l' || major_str.front() == L'L')) {
          major_str.remove_prefix(1);
        }

        auto major = convert_version_symbols_to_integral<uint16_t>(major_str);
        auto minor = convert_version_symbols_to_integral<uint16_t>(minor_str);
        auto micro = convert_version_symbols_to_integral<uint16_t>(micro_str);

        return api_set_version{major, minor, micro};
      }

      // Helper functions, will be useful to compare with forwarder_string,
      // since forwarder_string only stores narrow strings, while
      // api_set name is always specified in wide strings only.
      // Both, however, always store ASCII strings.

      [[nodiscard]] auto equals_to(std::string_view narrow_name) const noexcept {
        return compare_wide_with_narrow(m_name, narrow_name);
      }

      [[nodiscard]] auto clean_equals_to(std::string_view clean_narrow_name) {
        return compare_wide_with_narrow(clean_name(), clean_narrow_name);
      }

     private:
      [[nodiscard]] bool compare_wide_with_narrow(std::wstring_view wide,
                                                  std::string_view narrow) const noexcept {
        if (narrow.size() != wide.size())
          return false;

        auto comparator = [](char a, wchar_t b) {
          return a == static_cast<char>(b);
        };

        return std::ranges::equal(narrow, wide, comparator);
      }

      std::wstring_view m_name;
    };

    struct api_set_host_entry {
      std::wstring_view value;
      std::wstring_view alias;

      [[nodiscard]] auto host_present() const noexcept {
        return !value.empty();
      }
      [[nodiscard]] auto alias_present() const noexcept {
        return !alias.empty();
      }
    };

    class api_set_host_range {
     public:
      api_set_host_range(const win::api_set_value_entry* entries, uint32_t count, address_t base)
          : m_entries(entries), m_count(count), m_base(base) {}

      class iterator {
       public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = api_set_host_entry;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() : m_entries(nullptr), m_index(0), m_count(0), m_base(0) {}

        iterator(const win::api_set_value_entry* entries, uint32_t count, address_t base,
                 uint32_t index = 0)
            : m_entries(entries), m_index(index), m_count(count), m_base(base) {
          update_value();
        }

        reference operator*() noexcept {
          return m_current;
        }

        pointer operator->() noexcept {
          return &m_current;
        }

        iterator& operator++() {
          if (m_index < m_count) {
            ++m_index;
            update_value();
          }
          return *this;
        }

        iterator operator++(int) {
          iterator tmp = *this;
          ++(*this);
          return tmp;
        }

        bool operator==(const iterator& other) const {
          return m_entries == other.m_entries && m_index == other.m_index &&
                 m_count == other.m_count && m_base == other.m_base;
        }

        bool operator!=(const iterator& other) const {
          return !(*this == other);
        }

       private:
        void update_value() {
          if (!m_entries || m_index >= m_count) {
            m_current = {};
            return;
          }

          const auto& entry = m_entries[m_index];

          auto value_string_ptr = m_base.offset<wchar_t*>(entry.value_offset);
          auto value_string_length = static_cast<uint16_t>(entry.value_length / sizeof(wchar_t));
          std::wstring_view value{value_string_ptr, value_string_length};

          std::wstring_view alias;
          if (entry.name_length != 0) {
            auto alias_string_ptr = m_base.offset<wchar_t*>(entry.name_offset);
            auto alias_string_length = static_cast<uint16_t>(entry.name_length / sizeof(wchar_t));
            alias = {alias_string_ptr, alias_string_length};
          }

          m_current = {value, alias};
        }

        const win::api_set_value_entry* m_entries;
        uint32_t m_index;
        uint32_t m_count;
        address_t m_base;
        value_type m_current;
      };

      iterator begin() const {
        return iterator(m_entries, m_count, m_base, 0);
      }
      iterator end() const {
        return iterator(m_entries, m_count, m_base, m_count);
      }

      uint32_t size() const noexcept {
        return m_count;
      }

     private:
      const win::api_set_value_entry* m_entries;
      uint32_t m_count;
      address_t m_base;
    };

    class api_set_entry {
     public:
      api_set_entry() : m_value_entries(nullptr), m_value_count(0), m_base(0), m_sealed(false) {}

      api_set_entry(std::wstring_view contract_name, bool sealed,
                    const win::api_set_value_entry* entries, uint32_t count, address_t base)
          : m_contract(contract_name),
            m_sealed(sealed),
            m_value_entries(entries),
            m_value_count(count),
            m_base(base) {}

      // Contract, for example: "api-ms-win-core-com-l1-1-0"
      [[nodiscard]] auto contract() const noexcept {
        return m_contract;
      }
      [[nodiscard]] auto sealed() const noexcept {
        return m_sealed;
      }
      [[nodiscard]] auto host_count() const noexcept {
        return m_value_count;
      }
      [[nodiscard]] auto hosts() const {
        return api_set_host_range(m_value_entries, m_value_count, m_base);
      }

     private:
      api_set_contract m_contract;
      bool m_sealed;

      const win::api_set_value_entry* m_value_entries;
      uint32_t m_value_count;
      address_t m_base;
    };

    class api_set_map_view {
     public:
      api_set_map_view() {
        auto peb = win::PEB::address();

        m_api_set_map = reinterpret_cast<const win::api_set_namespace*>(peb->reserved9[0]);
        if (!m_api_set_map) {
          m_count = 0;
          m_first_entry = nullptr;
          m_base = 0;
          return;
        }
        m_count = m_api_set_map->count;
        m_base = reinterpret_cast<uintptr_t>(m_api_set_map);

        m_first_entry =
            m_base.offset<const win::api_set_namespace_entry*>(m_api_set_map->entry_offset);
      }

      class iterator {
       public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = api_set_entry;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() : m_enumerator(nullptr), m_index(0) {}

        iterator(const api_set_map_view* enumerator, uint32_t index)
            : m_enumerator(enumerator), m_index(index) {
          update_value();
        }

        reference operator*() noexcept {
          return m_current;
        }

        pointer operator->() noexcept {
          return &m_current;
        }

        iterator& operator++() noexcept {
          if (m_index < m_enumerator->m_count) {
            ++m_index;
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
          if (m_index > 0) {
            --m_index;
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
          return m_enumerator == other.m_enumerator && m_index == other.m_index;
        }

        bool operator!=(const iterator& other) const noexcept {
          return !(*this == other);
        }

       private:
        void update_value() {
          if (!m_enumerator || m_index >= m_enumerator->m_count) {
            m_current = api_set_entry();
            return;
          }

          auto ns_entry = m_enumerator->m_first_entry + m_index;

          constexpr auto api_set_schema_entry_flags_sealed = 1;
          bool sealed = (ns_entry->flags & api_set_schema_entry_flags_sealed) != 0;
          auto name_string_ptr = m_enumerator->m_base.offset<wchar_t*>(ns_entry->name_offset);
          auto name_string_length = static_cast<uint16_t>(ns_entry->name_length / sizeof(wchar_t));
          std::wstring_view contract_name{name_string_ptr, name_string_length};

          auto value_entry =
              m_enumerator->m_base.offset<const win::api_set_value_entry*>(ns_entry->value_offset);

          m_current = api_set_entry(contract_name, sealed, value_entry, ns_entry->value_count,
                                    m_enumerator->m_base);
        }

        const api_set_map_view* m_enumerator;
        uint32_t m_index;
        value_type m_current;
      };

      iterator begin() const noexcept {
        return iterator(this, 0);
      }
      iterator end() const noexcept {
        return iterator(this, m_count);
      }

      [[nodiscard]] uint32_t size() const noexcept {
        return m_count;
      }

      [[nodiscard]] iterator find(hash64_t contract_name_hash) const noexcept {
        for (auto it = begin(); it != end(); ++it) {
          auto full_name_hash = hash64_t{}(it->contract().name());
          if (full_name_hash == contract_name_hash)
            return it;

          auto clean_name_hash = hash64_t{}(it->contract().clean_name());
          if (clean_name_hash == contract_name_hash)
            return it;
        }
        return end();
      }

      [[nodiscard]] iterator
      find_if(std::predicate<iterator::value_type> auto pred) const noexcept {
        for (auto it = begin(); it != end(); ++it) {
          if (pred(*it))
            return it;
        }
        return end();
      }

     private:
      const win::api_set_namespace* m_api_set_map;
      const win::api_set_namespace_entry* m_first_entry;
      uint32_t m_count;
      address_t m_base;
    };

    class export_view {
     public:
      explicit export_view(address_t base) noexcept
          : m_module_base(base), m_export_dir(get_export_directory(base)) {}

      [[nodiscard]] std::size_t size() const noexcept {
        return m_export_dir->num_names;
      }

      [[nodiscard]] const win::export_directory_t* directory() const noexcept {
        return m_export_dir;
      }

      [[nodiscard]] auto name(std::size_t index) const noexcept {
        const auto rva_names_ptr = m_module_base.offset(m_export_dir->rva_names);
        const auto rva_names_span = rva_names_ptr.span<const uint32_t>(m_export_dir->num_names);
        const auto export_name_ptr = m_module_base.offset<const char*>(rva_names_span[index]);
        return std::string_view{export_name_ptr};
      }

      [[nodiscard]] auto ordinal(std::size_t index) const noexcept {
        // The ordinal table stores *unbiased* ordinals, so we add base.
        const auto ordinal_table_ptr = m_export_dir->ordinal_table(m_module_base);
        return static_cast<uint32_t>(m_export_dir->base + ordinal_table_ptr[index]);
      }

      [[nodiscard]] auto address(std::size_t index) const noexcept {
        const auto rva_table_ptr = m_export_dir->rva_table(m_module_base);
        const auto ordinal_table_ptr = m_export_dir->ordinal_table(m_module_base);

        const auto ordinal = ordinal_table_ptr[index];
        const auto rva_function = rva_table_ptr[ordinal];

        return m_module_base.offset(rva_function);
      }

      [[nodiscard]] auto is_export_forwarded(address_t export_address) const noexcept {
        const auto image = win::image_from_base(m_module_base);
        const auto export_data_dir =
            image->get_optional_header()->data_directories.export_directory;

        const auto export_table_start = m_module_base.offset(export_data_dir.rva);
        const auto export_table_end = export_table_start.offset(export_data_dir.size);

        return (export_address >= export_table_start) && (export_address < export_table_end);
      }

      class iterator {
       public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = win::export_t;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() : m_exports(nullptr), m_index(0), m_value({}) {};
        ~iterator() = default;
        iterator(const iterator&) = default;
        iterator(iterator&&) = default;
        iterator& operator=(iterator&&) = default;

        iterator(const export_view* exports, std::size_t index) noexcept
            : m_exports(exports), m_index(index), m_value() {
          update_value();
        }

        reference operator*() const noexcept {
          return m_value;
        }

        pointer operator->() const noexcept {
          return &m_value;
        }

        iterator& operator=(const iterator& other) noexcept {
          if (this != &other) {
            m_index = other.m_index;
            m_value = other.m_value;
          }
          return *this;
        }

        iterator& operator++() noexcept {
          if (m_index < m_exports->size()) {
            ++m_index;

            if (m_index < m_exports->size())
              update_value();
            else
              reset_value();
          } else {
            reset_value();
          }
          return *this;
        }

        iterator operator++(int) noexcept {
          iterator temp = *this;
          ++(*this);
          return temp;
        }

        iterator& operator--() noexcept {
          if (m_index > 0) {
            --m_index;
            update_value();
          }
          return *this;
        }

        iterator operator--(int) noexcept {
          iterator temp = *this;
          --(*this);
          return temp;
        }

        bool operator==(const iterator& other) const noexcept {
          return m_index == other.m_index && m_exports == other.m_exports;
        }

        bool operator!=(const iterator& other) const noexcept {
          return !(*this == other);
        }

       private:
        void update_value() noexcept {
          if (m_index < m_exports->size()) {
            const auto address = m_exports->address(m_index);
            m_value = value_type{
                .name = m_exports->name(m_index),
                .address = address,
                .ordinal = m_exports->ordinal(m_index),
                .is_forwarded = m_exports->is_export_forwarded(address),
            };
          }
        }

        void reset_value() noexcept {
          m_value = value_type{};
        }

        const export_view* m_exports;
        std::size_t m_index;
        mutable value_type m_value;
      };

      // Make sure the iterator is compatible with std::ranges
      static_assert(std::bidirectional_iterator<iterator>);

      iterator begin() const noexcept {
        return iterator(this, 0);
      }
      iterator end() const noexcept {
        return iterator(this, size());
      }

      // \brief Finds an export in the specified module.
      // \param export_name The name of the export to find.
      // \return Iterator pointing to [name, address] if export is found, or .end() if export is not found.
      [[nodiscard]] iterator find(hash64_t export_name) const noexcept {
        if (export_name == 0)
          return end();

        auto it = std::ranges::find_if(*this, [export_name](const win::export_t& data) -> bool {
          return export_name == hash64_t{}(data.name);
        });

        return it;
      }

      // \brief Find an export with user-defined predicate
      // \param predicate User-defined predicate
      // \return Iterator pointing to [name, address] if export is found, .end() if export is not found.
      [[nodiscard]] iterator find_if(std::predicate<iterator::value_type> auto predicate) const {
        return std::ranges::find_if(*this, predicate);
      }

     private:
      win::export_directory_t* get_export_directory(address_t base_address) const noexcept {
        const auto image = win::image_from_base(base_address.get());
        const auto export_data_dir =
            image->get_optional_header()->data_directories.export_directory;
        return m_module_base.offset<win::export_directory_t*>(export_data_dir.rva);
      }

      address_t m_module_base;
      win::export_directory_t* m_export_dir{nullptr};
    };

    class dynamic_link_library {
     public:
      constexpr dynamic_link_library() noexcept = default;
      dynamic_link_library(hash64_t module_name) : m_data(find(module_name).loader_table_entry()) {}
      dynamic_link_library(win::loader_table_entry* module_data) : m_data(module_data) {}

      dynamic_link_library(const dynamic_link_library& instance) = default;
      dynamic_link_library(dynamic_link_library&& instance) = default;
      dynamic_link_library& operator=(const dynamic_link_library& instance) = default;
      dynamic_link_library& operator=(dynamic_link_library&& instance) = default;
      ~dynamic_link_library() = default;

      // \return loader_table_entry* - Raw pointer to Win32
      // loader data about the current module
      [[nodiscard]] win::loader_table_entry* loader_table_entry() const noexcept {
        return m_data;
      }

      // \return image_t - Displaying an image in process
      // memory using the image_t structure from "linuxpe"
      [[nodiscard]] auto image() const noexcept {
        return win::image_from_base(m_data);
      }

      // \return uint16_t - How many references the current
      // DLL has at the moment
      [[nodiscard]] auto reference_count() const noexcept {
        return loader_table_entry()->obsolete_load_count;
      }

      // \return address_t - Base address of current DLL
      [[nodiscard]] auto base_address() const noexcept {
        return m_data->base_address;
      }

      // \return void* - Pointer on base address of current
      // DLL, same as GetModuleHandle() in Win32 API
      [[nodiscard]] auto native_handle() const noexcept {
        return base_address().ptr();
      }

      // \return Address of entrypoint
      [[nodiscard]] auto entry_point() const noexcept {
        return m_data->entry_point;
      }

      // \return Name of current DLL as std::wstring_view
      [[nodiscard]] auto name() const noexcept {
        return m_data == nullptr ? win::unicode_string{} : m_data->name;
      }

      // \return Filepath to current DLL as std::wstring_view
      [[nodiscard]] auto filepath() const noexcept {
        return m_data == nullptr ? win::unicode_string{} : m_data->path;
      }

      // \return Exports range-enumerator of current DLL
      [[nodiscard]] auto exports() const noexcept {
        return export_view{m_data->base_address};
      }

      template <std::integral Ty = std::size_t>
      [[nodiscard]] auto section_checksum(hash32_t section_name = ".text") const {
        const auto module_base = base_address();
        const auto sections = image()->get_nt_headers()->sections();
        auto section = std::find_if(sections.begin(), sections.end(),
                                    [=](const win::section_header_t& section) {
                                      return section_name == hash32_t{}(section.name.view());
                                    });

        const auto section_content =
            std::span{module_base.ptr<uint8_t>(section->virtual_address), section->virtual_size};
        return memory_checksum<Ty>{section_content}.result();
      }

      [[nodiscard]] auto present() const noexcept {
        return m_data != nullptr;
      }

      [[nodiscard]] bool operator==(const dynamic_link_library& other) const noexcept {
        return m_data == other.m_data;
      }

      [[nodiscard]] bool operator==(hash64_t module_name_hash) const noexcept {
        const auto module_name = name().view();
        // Try to compare hash of full module name
        const auto full_name_hash = hash64_t{}(module_name);

        if (module_name.size() <= 4)
          return module_name_hash == full_name_hash;

        // Try to compare hash of trimmed module name (.dll)
        const auto trimmed_name = module_name.substr(0, module_name.size() - 4);
        const auto trimmed_name_hash = hash64_t{}(trimmed_name);

        // Verify both hashes
        return full_name_hash == module_name_hash || trimmed_name_hash == module_name_hash;
      }

      [[nodiscard]] explicit operator bool() const noexcept {
        return present();
      }

      friend std::ostream& operator<<(std::ostream& os, const dynamic_link_library& dll) {
        return os << dll.name();
      }

     private:
      dynamic_link_library find(hash64_t module_name) const;
      win::loader_table_entry* m_data{nullptr};
    };

    class module_view {
     public:
      explicit module_view() {
        auto entry = &win::PEB::loader_data()->in_load_order_module_list;
        m_begin = entry->flink;
        m_end = entry;
      }

      module_view& skip_module() {
        m_begin = m_begin->flink;
        return *this;
      }

      class iterator {
       public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = dynamic_link_library;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

        iterator() noexcept(std::is_nothrow_default_constructible_v<value_type>)
            : m_entry(nullptr), m_value({}) {}
        ~iterator() = default;
        iterator(const iterator&) = default;
        iterator(iterator&&) = default;
        iterator& operator=(iterator&&) = default;
        iterator(win::list_entry* entry) : m_entry(entry) {
          on_update();
        }

        pointer operator->() const noexcept {
          return &m_value;
        }

        iterator& operator=(const iterator& other) noexcept {
          if (this != &other) {
            m_entry = other.m_entry;
            on_update();
          }
          return *this;
        }

        iterator& operator++() noexcept {
          m_entry = m_entry->flink;
          on_update();
          return *this;
        }

        iterator operator++(int) noexcept {
          iterator temp = *this;
          ++(*this);
          return temp;
        }

        iterator& operator--() noexcept {
          m_entry = m_entry->blink;
          on_update();
          return *this;
        }

        iterator operator--(int) noexcept {
          iterator temp = *this;
          --(*this);
          return temp;
        }

        bool operator==(const iterator& other) const noexcept {
          return m_entry == other.m_entry;
        }

        bool operator!=(const iterator& other) const noexcept {
          return !(*this == other);
        }

        reference operator*() const noexcept {
          return m_value;
        }

       private:
        void on_update() const noexcept {
          auto table_entry =
              win::containing_record(m_entry, &win::loader_table_entry::in_load_order_links);
          m_value = dynamic_link_library{table_entry};
        }

        win::list_entry* m_entry;
        mutable value_type m_value;
      };

      // Make sure the iterator is compatible with std::ranges
      static_assert(std::bidirectional_iterator<iterator>);

      iterator begin() const noexcept {
        return iterator(m_begin);
      }
      iterator end() const noexcept {
        return iterator(m_end);
      }

      // \brief Find an export with user-defined predicate
      // \param predicate User-defined predicate
      // \return Iterator pointing to [name, address] if
      // export is found, .end() if export is not found.
      [[nodiscard]] iterator
      find_if(std::predicate<iterator::value_type> auto predicate) const noexcept {
        return std::ranges::find_if(*this, predicate);
      }

     private:
      win::list_entry* m_begin{nullptr};
      win::list_entry* m_end{nullptr};
    };

    struct use_ordinal_t {
      explicit constexpr use_ordinal_t() noexcept = default;
    };

    class exported_symbol {
     public:
      exported_symbol() = default;
      explicit exported_symbol(hash64_t export_name, hash64_t module_hash = 0) noexcept
          : m_data(find_export_address(export_name, module_hash)) {}

      explicit exported_symbol(hash64_t module_hash, std::uint32_t ordinal) noexcept
          : m_data(find_export_by_ordinal(module_hash, ordinal)) {}

      exported_symbol(const exported_symbol& instance) = default;
      exported_symbol(exported_symbol&& instance) = default;
      exported_symbol& operator=(const exported_symbol& instance) = default;
      exported_symbol& operator=(exported_symbol&& instance) = default;
      ~exported_symbol() = default;

      [[nodiscard]] auto address() const noexcept {
        return m_data.address;
      }
      [[nodiscard]] auto location() const noexcept {
        return m_data.dll;
      }
      [[nodiscard]] auto is_forwarder_unresolved() const noexcept {
        return m_data.is_forwarded;
      }
      [[nodiscard]] auto forwarder_string() const noexcept {
        return m_data.forwarder_string;
      }
      [[nodiscard]] auto present() const noexcept {
        return static_cast<bool>(m_data.address);
      }

      [[nodiscard]] bool operator==(address_t other) const noexcept {
        return m_data.address == other;
      }

      [[nodiscard]] explicit operator bool() const noexcept {
        return present();
      }
      [[nodiscard]] explicit operator address_t() const noexcept {
        return address();
      }

      friend std::ostream& operator<<(std::ostream& os, const exported_symbol& exp) {
        os << exp.address().ptr();
        if (exp.location().name())
          os << ':' << exp.location().name();
        return os;
      }

     private:
      struct export_with_location {
        address_t address{0};
        detail::dynamic_link_library dll{};
        bool is_forwarded{false};
        win::forwarder_string forwarder_string{};
      };

      export_with_location find_export_by_ordinal(hash64_t module_hash,
                                                  std::uint32_t ordinal) const noexcept {
        if (module_hash == 0)
          return {};

        const auto process_modules = module_view{}.skip_module();

        // Enumerate every module loaded to process
        for (const auto& module : process_modules) {
          if (module != module_hash)
            continue;

          export_view exports{module.base_address()};

          // Search for export by comparing ordinals
          const auto predicate_by_name = [ordinal](const win::export_t& exp) -> bool {
            return exp.ordinal == ordinal;
          };

          if (auto export_it = exports.find_if(predicate_by_name); export_it != exports.end()) {
            const win::export_t& export_data = *export_it;

            // Learn more here: https://devblogs.microsoft.com/oldnewthing/20060719-24/?p=30473
            if (export_data.is_forwarded) {
              return handle_forwarded_export(export_data.address);
            }

            return {export_data.address, module};
          }
        }

        return {};
      }

      export_with_location find_export_address(hash64_t export_name,
                                               hash64_t module_hash = 0) const noexcept {
        if (export_name == 0)
          return {};

        const auto process_modules = module_view{}.skip_module();
        const bool is_module_specified = module_hash != 0;

        // Enumerate every module loaded to process
        for (const auto& module : process_modules) {
          if (is_module_specified && module != module_hash)
            continue;

          export_view exports{module.base_address()};

          // Search for export by comparing hashed names
          const auto predicate_by_name = [export_name](const win::export_t& data) -> bool {
            return export_name == hash64_t{}(data.name);
          };

          if (auto export_it = exports.find_if(predicate_by_name); export_it != exports.end()) {
            const win::export_t& export_data = *export_it;

            // Learn more here: https://devblogs.microsoft.com/oldnewthing/20060719-24/?p=30473
            if (export_data.is_forwarded) {
              return handle_forwarded_export(export_data.address);
            }

            return {export_data.address, module};
          }
        }

        return {};
      }

      export_with_location handle_forwarded_export(address_t address) const {
        // In a forwarded export, the address is a string containing
        // information about the actual export and its location
        // They are always presented as "module_name.export_name"
        auto fwd_str = address.ptr<const char>();

        // Split forwarded export to module name and real export name
        auto [module_name, token] = win::split_forwarder_string(fwd_str, '.');

        // Perform call with the name of the real export, with a pre-known module
        export_with_location real_export;
        if (!token.empty() && token.front() == '#') {
          auto ordinal_str = token.substr(1);
          std::uint32_t real_ordinal = 0;
          auto conversion_result = std::from_chars(
              ordinal_str.data(), ordinal_str.data() + ordinal_str.size(), real_ordinal);

          if (conversion_result.ec == std::errc{})
            real_export = find_export_by_ordinal(hash64_t{}(module_name), real_ordinal);
        } else {
          real_export = find_export_address(hash64_t{}(token), hash64_t{}(module_name));
        }

        // The DLL pointed to by the forwarder is not loaded into the
        // process, so we just return the result with forwarder_string
        if (!real_export.dll.present()) {
          return export_with_location{
              .is_forwarded = true,
              .forwarder_string = {module_name, token},
          };
        }

        return real_export;
      }

      export_with_location m_data;
    };

    inline dynamic_link_library dynamic_link_library::find(hash64_t module_name) const {
      module_view modules{};
      auto it = modules.find_if([=, this](const dynamic_link_library& dll) -> bool {
        return !dll.name().view().empty() && dll == module_name;
      });
      return it != modules.end() ? *it : dynamic_link_library{};
    }

    class operating_system {
     public:
      operating_system(std::uint32_t major, std::uint32_t minor, std::uint32_t build_num) noexcept
          : m_major_version(major), m_minor_version(minor), m_build_number(build_num) {}

      [[nodiscard]] auto is_windows_11() const noexcept {
        return m_major_version == 10 && m_build_number >= 22000;
      }

      [[nodiscard]] auto is_windows_10() const {
        return m_major_version == 10 && m_build_number < 22000;
      }

      [[nodiscard]] auto is_windows_8_1() const noexcept {
        return verify_version_mask(6, 3);
      }
      [[nodiscard]] auto is_windows_8() const noexcept {
        return verify_version_mask(6, 2);
      }
      [[nodiscard]] auto is_windows_7() const noexcept {
        return verify_version_mask(6, 1);
      }
      [[nodiscard]] auto is_windows_xp() const noexcept {
        return verify_version_mask(6, 0);
      }
      [[nodiscard]] auto is_windows_vista() const noexcept {
        return verify_version_mask(5, 1);
      }
      [[nodiscard]] auto major_version() const noexcept {
        return m_major_version;
      }
      [[nodiscard]] auto minor_version() const noexcept {
        return m_minor_version;
      }
      [[nodiscard]] auto build_number() const noexcept {
        return m_build_number;
      }

      [[nodiscard]] auto formatted() const noexcept {
        return std::format("Windows {}.{} (Build {})", m_major_version, m_minor_version,
                           m_build_number);
      }

     private:
      bool verify_version_mask(std::uint32_t major, std::uint32_t minor) const {
        return m_major_version == major && m_minor_version == minor;
      }

      std::uint32_t m_major_version, m_minor_version, m_build_number;
    };

    class time_formatter {
     public:
      constexpr time_formatter(std::uint64_t unix_timestamp) noexcept
          : m_unix_seconds(unix_timestamp) {}

      // \return European format: "dd.mm.yyyy hh:mm"
      [[nodiscard]] auto format_european() const {
        auto [year, month, day, hours, minutes, _] = break_down_unix_time(m_unix_seconds);
        return std::format("{:02}.{:02}.{} {:02}:{:02}", day, month, year, hours, minutes);
      }

      // \return American format: "mm/dd/yyyy hh:mm"
      [[nodiscard]] auto format_american() const {
        auto [year, month, day, hours, minutes, _] = break_down_unix_time(m_unix_seconds);
        return std::format("{:02}/{:02}/{} {:02}:{:02}", month, day, year, hours, minutes);
      }

      // \return ISO 8601 format: "yyyy-mm-ddThh:mm:ss"
      [[nodiscard]] auto format_iso8601() const {
        auto [year, month, day, hours, minutes, seconds] = break_down_unix_time(m_unix_seconds);
        return std::format("{}-{:02}-{:02}T{:02}:{:02}:{:02}", year, month, day, hours, minutes,
                           seconds);
      }

      // \return Raw unix timestamp as integral
      [[nodiscard]] auto time_since_epoch() const noexcept {
        return m_unix_seconds;
      }

      operator std::uint64_t() const noexcept {
        return m_unix_seconds;
      }

     private:
      struct timestamp {
        int32_t year;
        uint32_t month;
        uint32_t day;
        int32_t hours;
        int32_t minutes;
        int32_t seconds;
      };

      timestamp break_down_unix_time(std::uint64_t unix_timestamp) const {
        auto time_point =
            std::chrono::system_clock::time_point(std::chrono::seconds(unix_timestamp));

        auto days = std::chrono::floor<std::chrono::days>(time_point);
        auto time_since_midnight =
            std::chrono::duration_cast<std::chrono::seconds>(time_point - days);

        std::chrono::year_month_day ymd{days};
        timestamp stamp{
            .year = static_cast<int32_t>(ymd.year()),
            .month = static_cast<uint32_t>(ymd.month()),
            .day = static_cast<uint32_t>(ymd.day()),
            .hours = static_cast<int32_t>(
                std::chrono::duration_cast<std::chrono::hours>(time_since_midnight).count()),
            .minutes = static_cast<int32_t>(
                std::chrono::duration_cast<std::chrono::minutes>(time_since_midnight).count() % 60),
            .seconds = static_cast<int32_t>(time_since_midnight.count() % 60)};

        return stamp;
      }

      std::uint64_t m_unix_seconds;
    };

    class zoned_time {
     public:
      constexpr zoned_time(std::uint64_t unix_timestamp, std::int64_t timezone_offset) noexcept
          : m_unix_seconds(unix_timestamp), m_timezone_offset(timezone_offset) {}

      [[nodiscard]] auto utc() const noexcept {
        return time_formatter{m_unix_seconds};
      }

      [[nodiscard]] auto local() const noexcept {
        return time_formatter{m_unix_seconds + m_timezone_offset};
      }

      operator std::uint64_t() const noexcept {
        return m_unix_seconds;
      }

     private:
      std::uint64_t m_unix_seconds;
      std::int64_t m_timezone_offset;
    };

    // shared_data parses kernel_user_shared_data filled
    // by the operating system when the process starts.
    // The structure contains a lot of useful information
    // about the operating system. The class is a high-level
    // wrapper for parsing, which will save you from direct
    // work with raw addresses and can greatly simplify
    // your coding process.
    class shared_data {
     public:
      // The read-only user-mode address for the shared data
      // is 0x7ffe0000, both in 32-bit and 64-bit Windows.
      constexpr static shadow::address_t memory_location{0x7ffe0000};

      // The difference in epochs depicted in seconds between
      // "January 1st, 1601" and "January 1st, 1970".
      constexpr static std::chrono::seconds epoch_difference{0x2b6109100};

      // Windows time is always represented as 100-nanosecond
      // interval. Define a type to easily convert through.
      using hundred_ns_interval = std::chrono::duration<int64_t, std::ratio<1, 10000000>>;

     public:
      constexpr shared_data() : m_data(memory_location.ptr<win::kernel_user_shared_data>()) {}

      [[nodiscard]] auto* get() const noexcept {
        return m_data;
      }

      [[nodiscard]] auto kernel_debugger_present() const noexcept {
        return m_data->kernel_debugger_enabled;
      }

      [[nodiscard]] auto safe_boot_enabled() const noexcept {
        return m_data->safe_boot_mode;
      }

      [[nodiscard]] auto boot_id() const noexcept {
        return m_data->boot_id;
      }

      [[nodiscard]] auto physical_pages_num() const noexcept {
        return m_data->number_of_physical_pages;
      }

      [[nodiscard]] auto system_root() const noexcept {
        const std::wstring_view wstr{m_data->nt_system_root};
        auto string_pointer = const_cast<wchar_t*>(wstr.data());
        auto string_size = static_cast<std::uint16_t>(wstr.size());
        return win::unicode_string{string_pointer, string_size};
      }

      [[nodiscard]] auto timezone_id() const noexcept {
        return m_data->time_zone_id;
      }

      template <concepts::chrono_duration Ty>
      [[nodiscard]] auto timezone_offset() const noexcept(std::is_nothrow_constructible_v<Ty>) {
        std::chrono::seconds seconds{parse_time_zone_bias()};
        return std::chrono::duration_cast<Ty>(seconds);
      }

      [[nodiscard]] auto system() const {
        const auto major = m_data->nt_major_version;
        const auto minor = m_data->nt_minor_version;
        const auto build_num = m_data->nt_build_number;
        return operating_system{major, minor, build_num};
      }

      // \return 100-ns interval. Timestamp starting
      // from Windows epoch, "January 1st, 1601"
      [[nodiscard]] auto windows_epoch_timestamp() const {
        const auto system_time = m_data->system_time;
        const auto windows_time_100ns =
            static_cast<uint64_t>(system_time.high1_time) << 32 | system_time.low_part;
        return windows_time_100ns;
      }

      // \return Seconds. Timestamp starting from
      // Unix epoch, "January 1st, 1970"
      [[nodiscard]] auto unix_epoch_timestamp() const {
        // Windows time is measured in 100-nanosecond intervals.
        // Convert 100-ns intervals to seconds, formula is:
        // 1 second = 10,000,000 100-ns intervals
        const auto windows_time_100ns = windows_epoch_timestamp();
        const auto windows_time_seconds = std::chrono::duration_cast<std::chrono::seconds>(
            hundred_ns_interval(windows_time_100ns));
        return zoned_time{static_cast<uint64_t>((windows_time_seconds - epoch_difference).count()),
                          parse_time_zone_bias()};
      }

     private:
      std::int64_t parse_time_zone_bias() const {
        const auto bias = m_data->time_zone_bias;
        // Build 64-bit value from low_part and high1_time
        const auto bias_100ns = (static_cast<std::int64_t>(bias.high1_time) << 32) | bias.low_part;

        // The time offset is measured from local time to UTC,
        // in 100-ns intervals. Convert 100-ns intervals to
        // seconds (1 second = 10,000,000 100-ns intervals)
        const auto bias_seconds =
            std::chrono::duration_cast<std::chrono::seconds>(hundred_ns_interval(bias_100ns));

        // Offset from local time to UTC: if the offset is
        // positive, it means UTC is ahead, so the result
        // should be made negative.
        return -bias_seconds.count();
      }

      win::kernel_user_shared_data* m_data;
    };

#ifndef SHADOWSYSCALLS_DISABLE_CACHING

    template <typename ValueTy, typename KeyTy>
    class memory_cache {
     public:
      using value_t = ValueTy;
      using key_t = KeyTy;

      value_t operator[](key_t export_hash) {
        std::shared_lock lock(m_cache_mutex);
        auto it = m_cache_map.find(export_hash);
        return it == m_cache_map.end() ? value_t{} : it->second;
      }

      void emplace(key_t key, value_t value) {
        std::scoped_lock lock(m_cache_mutex);
        m_cache_map.emplace(key, value);
      }

      bool try_emplace(key_t key, value_t value) {
        std::scoped_lock lock(m_cache_mutex);
        const bool was_emplaced = m_cache_map.try_emplace(key, value).second;
        return was_emplaced;
      }

      void erase(key_t key) {
        std::scoped_lock lock(m_cache_mutex);
        m_cache_map.erase(key);
      }

      void clear() {
        std::scoped_lock lock(m_cache_mutex);
        m_cache_map.clear();
      }

      bool exists(key_t key) {
        std::shared_lock lock(m_cache_mutex);
        return m_cache_map.find(key) != m_cache_map.end();
      }

      std::size_t size() const {
        std::shared_lock lock(m_cache_mutex);
        return m_cache_map.size();
      }

      auto begin() {
        std::shared_lock lock(m_cache_mutex);
        return m_cache_map.begin();
      }

      auto end() {
        std::shared_lock lock(m_cache_mutex);
        return m_cache_map.end();
      }

     private:
      // Making sure that's every `cache_map` call is safe.
      mutable std::shared_mutex m_cache_mutex{};
      std::unordered_map<key_t, value_t> m_cache_map{};
    };

    inline memory_cache<std::uint32_t, hash64_t::underlying_t> ssn_cache;
    inline memory_cache<detail::exported_symbol, hash64_t::underlying_t> address_cache;

#endif

    template <typename Ty>
    auto convert_nulls_to_nullptrs(Ty arg) {
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
      // lower half of a 64 - bit register zeroes the upper part too
      // ( i.e.ecx = 0 = > rcx = 0 ), so this problem should only exist
      // on x64 for arguments after the fourth.
      // The solution would be on templates to loop through all
      // arguments and manually cast them to size_t size.

      constexpr auto is_signed_integral = std::signed_integral<Ty>;
      constexpr auto is_unsigned_integral = std::unsigned_integral<Ty>;

      using unsigned_integral_type = std::conditional_t<is_unsigned_integral, std::uintptr_t, Ty>;
      using tag_type =
          std::conditional_t<is_signed_integral, std::intptr_t, unsigned_integral_type>;

      return static_cast<tag_type>(arg);
    }

    template <auto Fn>
    consteval std::string_view extract_function_name() {
#if defined(__clang__)
      // "... extract_function_name() [Fn = &FunctionNameA]"
      constexpr std::string_view pretty = __PRETTY_FUNCTION__;

      // '&' is where a function name begins
      constexpr auto name_start = pretty.rfind('&') + 1;
      constexpr auto name_end = pretty.find(']');
      constexpr auto func_name = pretty.substr(name_start, name_end - name_start);
#elif defined(__GNUC__)
      // "... extract_function_name() [with auto Fn = MessageBoxA; std::string_view = std::basic_string_view<char>]"
      constexpr std::string_view pretty = __PRETTY_FUNCTION__;

      constexpr std::string_view marker{" auto Fn = "};
      constexpr auto name_start = pretty.find(marker) + marker.size();
      constexpr auto name_end = pretty.find(';');
      constexpr auto func_name = pretty.substr(name_start, name_end - name_start);
#elif defined(_MSC_VER)
      // "... extract_function_name<int __cdecl A::B::FunctionNameA(int, int*)>(void)"
      constexpr std::string_view sig{__FUNCSIG__};
      constexpr std::string_view marker{"extract_function_name<"};

      constexpr std::size_t after = sig.find(marker) + marker.size();  // "... A::B::FunctionNameA("
      constexpr std::size_t paren = sig.find('(', after);              // '(' of param list
      constexpr auto left_part =
          sig.substr(after, paren - after);  // "int __cdecl A::B::FunctionNameA"

      // points to last letter of the name
      constexpr std::size_t name_end = left_part.find_last_not_of(" \t");

      // last whitespace before the name (space or tab) " A::B::FunctionNameA"
      constexpr std::size_t sep = left_part.find_last_of(" \t", name_end);

      // begin of the (possibly qualified) identifier
      constexpr std::size_t name_begin = (sep == std::string_view::npos) ? 0 : sep + 1;

      // "A::B::FunctionNameA" or just "FunctionNameA"
      constexpr auto ident = left_part.substr(name_begin, name_end - name_begin + 1);

      // drop scope qualifier (namespace/class) if present
      // (it will never be the case with WinAPI functions, but anyway...)
      constexpr std::size_t scope = ident.rfind("::");
      constexpr auto func_name =
          (scope == std::string_view::npos) ? ident : ident.substr(scope + 2);
#else
#error Unsupported compiler
#endif
      static_assert(!func_name.empty(), "Failed to extract function name");
      return func_name;
    }

  }  // namespace detail

  namespace mem {
    constexpr std::uint32_t commit = 0x1000;
    constexpr std::uint32_t reserve = 0x2000;
    constexpr std::uint32_t commit_reserve = commit | reserve;
    constexpr std::uint32_t large_commit = commit | reserve | 0x20000000;
    constexpr std::uint32_t release = 0x8000;

    namespace page {
      constexpr std::uint32_t no_access = 0x00000001;
      constexpr std::uint32_t read_only = 0x00000002;
      constexpr std::uint32_t read_write = 0x00000004;
      constexpr std::uint32_t execute = 0x00000010;
      constexpr std::uint32_t execute_read = 0x00000020;
      constexpr std::uint32_t execute_read_write = 0x00000040;

      constexpr std::uint32_t guard = 0x00000100;
      constexpr std::uint32_t no_cache = 0x00000200;
      constexpr std::uint32_t write_combine = 0x00000400;
    }  // namespace page
  }  // namespace mem

  constexpr detail::use_ordinal_t use_ordinal{};
  using detail::hash32_t;
  using detail::hash64_t;

  namespace literals {

    consteval hash32_t operator""_h32(const char* str, std::size_t len) noexcept {
      return hash32_t{str, len};
    }

    consteval hash64_t operator""_h64(const char* str, std::size_t len) noexcept {
      return hash64_t{str, len};
    }

  }  // namespace literals

  // Used in `shadowcall` to create a pairing with simple syntax,
  // { "NtQueryInformationProcess", "ntdll" }
  struct hashpair {
    consteval hashpair(hash64_t first_, hash64_t second_) : first(first_), second(second_) {}

    hash64_t first;
    hash64_t second;
  };

  inline auto dll(hash64_t name) {
    return detail::dynamic_link_library{name};
  }

  inline auto base_module() {
    return *(detail::module_view{}.begin());
  }

  inline auto exported_symbol(hash64_t export_name, hash64_t module_name = 0) {
    return detail::exported_symbol{export_name, module_name};
  }

  inline auto exported_symbol(detail::use_ordinal_t, hash64_t module, std::uint32_t ordinal) {
    return detail::exported_symbol(module, ordinal);
  }

  inline auto dlls() {
    return detail::module_view{}.skip_module();
  }

  inline auto exported_symbols(hash64_t module_name) {
    return detail::export_view{dll(module_name).base_address()};
  }

  inline auto shared_data() {
    return detail::shared_data{};
  }

  inline auto api_set_map_view() {
    return detail::api_set_map_view{};
  }

  inline auto cpu() {
    static detail::cpu_info processor;
    return processor;
  }

  // nt_memory_allocator allocates memory based on "Nt" memory
  // functions located at "ntdll.dll".
  template <typename Ty, std::uint32_t AllocFlags = mem::commit_reserve,
            std::uint32_t Protect = mem::page::read_write>
  class nt_memory_allocator {
   public:
    using value_type = Ty;
    using pointer = Ty*;
    using const_pointer = const Ty*;
    using reference = Ty&;
    using const_reference = const Ty&;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    template <class U>
    struct rebind {
      using other = nt_memory_allocator<U, AllocFlags, Protect>;
    };

    template <class U, std::uint32_t AF, std::uint32_t PR>
    constexpr nt_memory_allocator(const nt_memory_allocator<U, AF, PR>&) noexcept
        : m_flags(AF), m_protect(PR) {}
    constexpr nt_memory_allocator() noexcept = default;

    template <typename U>
    constexpr nt_memory_allocator(const nt_memory_allocator<U>&) noexcept {}

    [[nodiscard]] Ty* allocate(std::size_t n) {
      std::size_t size = n * sizeof(Ty);
      void* ptr = virtual_alloc(nullptr, size, AllocFlags, Protect);
#if SHADOW_HAS_EXCEPTIONS
      if (!ptr)
        throw std::bad_alloc();
#endif
      return static_cast<Ty*>(ptr);
    }

    [[nodiscard]] Ty* allocate(address_t address, std::size_t n) {
      std::size_t size = n * sizeof(Ty);
      void* ptr = virtual_alloc(address.ptr(), size, AllocFlags, Protect);
#if SHADOW_HAS_EXCEPTIONS
      if (!ptr)
        throw std::bad_alloc();
#endif
      return static_cast<Ty*>(ptr);
    }

    template <typename PtrTy>
    void deallocate(PtrTy p, std::size_t n) noexcept {
      std::size_t size = n * sizeof(Ty);
      virtual_free(static_cast<void*>(p), size, mem::release);
    }

    friend bool operator==(nt_memory_allocator, nt_memory_allocator) noexcept {
      return true;
    }
    friend bool operator!=(nt_memory_allocator, nt_memory_allocator) noexcept {
      return false;
    }

    using is_always_equal = std::true_type;

   private:
    using NTSTATUS = long;

    void* virtual_alloc(void* address, std::uint64_t allocation_size, std::uint32_t allocation_t,
                        std::uint32_t protect) const {
      void* current_process{reinterpret_cast<void*>(-1)};
      void* base_address = address;
      std::uint64_t region_size = allocation_size;
      static address_t allocation_procedure{
          exported_symbol("NtAllocateVirtualMemory", "ntdll.dll").address()};

      auto result = allocation_procedure.execute<NTSTATUS>(
          current_process, &base_address, 0ull, &region_size, allocation_t & 0xFFFFFFC0, protect);
      return result >= 0 ? base_address : nullptr;
    }

    bool virtual_free(void* address, std::uint64_t allocation_size, std::uint32_t flags) const {
      NTSTATUS result{0};
      auto region_size{allocation_size};
      void* base_address = address;
      void* current_process{reinterpret_cast<void*>(-1)};
      static address_t free_procedure{
          exported_symbol("NtFreeVirtualMemory", "ntdll.dll").address()};

      if (((flags & 0xFFFF3FFC) != 0 || (flags & 0x8003) == 0x8000) && allocation_size)
        result = -0x3FFFFFF3;

      result =
          free_procedure.execute<NTSTATUS>(current_process, &base_address, &region_size, flags);
      if (result == -0x3FFFFFBB)
        result =
            free_procedure.execute<NTSTATUS>(current_process, &base_address, &region_size, flags);

      return result >= 0;
    }

    std::uint32_t m_flags = AllocFlags;
    std::uint32_t m_protect = Protect;
  };

  template <typename Ty>
  using rw_allocator = nt_memory_allocator<Ty, mem::commit_reserve, mem::page::read_write>;
  template <typename Ty>
  using rx_allocator = nt_memory_allocator<Ty, mem::commit_reserve, mem::page::execute_read>;
  template <typename Ty>
  using rwx_allocator = nt_memory_allocator<Ty, mem::commit_reserve, mem::page::execute_read_write>;
  template <typename Ty>
  using huge_allocator = nt_memory_allocator<Ty, mem::large_commit, mem::page::read_write>;

  template <std::uint32_t shell_size>
  class shellcode {
   public:
    template <class... Args>
      requires((std::is_convertible_v<Args, std::uint8_t> && ...) && shell_size != 0)
    shellcode(Args&&... list) noexcept
        : m_shellcode{static_cast<std::uint8_t>(std::forward<Args&&>(list))...} {}

    ~shellcode() {
      if (m_memory == nullptr) {
        return;
      }

      m_allocator.deallocate(m_memory, shell_size);
      m_memory = nullptr;
    }

    void setup() {
      m_memory = m_allocator.allocate(shell_size);
      if (m_memory != nullptr) {
        memcpy(m_memory, m_shellcode.data(), shell_size);
        m_shellcode_fn = m_memory;
      }
    }

    template <std::integral Ty = std::uint8_t>
    [[nodiscard]] constexpr Ty read(std::size_t index) const noexcept {
      return m_shellcode[index];
    }

    template <std::integral Ty>
    constexpr void write(std::size_t index, Ty value) noexcept {
      *reinterpret_cast<Ty*>(&m_shellcode[index]) = value;
    }

    template <typename Ty, typename... Args>
      requires(std::is_default_constructible_v<Ty>)
    [[nodiscard]] Ty execute(Args&&... args) const noexcept {
      if (!m_shellcode_fn) {
        return Ty{};
      }
      return reinterpret_cast<Ty(__stdcall*)(Args...)>(m_shellcode_fn)(args...);
    }

    template <typename Ty = void, typename PointerTy = std::add_pointer_t<Ty>>
    [[nodiscard]] constexpr PointerTy ptr() const noexcept {
      return static_cast<PointerTy>(m_shellcode_fn);
    }

   private:
    void* m_shellcode_fn = nullptr;
    void* m_memory = nullptr;
    rwx_allocator<std::uint8_t> m_allocator;
    std::array<std::uint8_t, shell_size> m_shellcode;
  };

  namespace error {

    // Names for generic error codes
    enum errc : std::uint32_t {
      none = 0,         // No error occured
      ssn_not_found,    // System Service Number can't be found
      export_not_found  // Such export doesn't exist
    };

  }  // namespace error

  template <concepts::fundamental Ty = long, bool IsTypeNtStatus = concepts::is_type_ntstatus<Ty>>
    requires(shadow::is_x64)
  class syscaller {
   public:
    // Parser needs to return std::optional<uint32_t> and accept (syscaller&, address_t)
    using ssn_parser_t = detail::stack_function<std::optional<uint32_t>(syscaller&, address_t)>;

   public:
    constexpr syscaller(hash64_t syscall_name) noexcept(!SHADOW_HAS_EXCEPTIONS)
        : m_name_hash(syscall_name),
          m_service_number(0),
          m_last_error(std::nullopt),
          m_ssn_parser([this]([[maybe_unused]] syscaller& instance, address_t address) {
            return this->default_ssn_parser(address);
          }) {}

    template <typename... Args>
    auto operator()(Args&&... args) noexcept {
      auto parse_result = resolve_service_number();
      if (!parse_result || m_last_error) {
        // Return -1 if type is NTSTATUS (call failed),
        // otherwise return default-constructible (0)
        return IsTypeNtStatus ? Ty{-1} : Ty{};
      } else {
        m_service_number = *parse_result;
      }
      setup_shellcode();
      return m_shellcode.execute<Ty>(shadow::detail::convert_nulls_to_nullptrs(args)...);
    }

    void set_ssn_parser(ssn_parser_t parser) {
      m_ssn_parser.swap(parser);
    }

    void set_last_error(std::uint32_t error) noexcept {
      m_last_error.emplace(error);
    }

    std::optional<std::uint32_t> last_error() const noexcept {
      return m_last_error;
    }

    explicit operator bool() const noexcept {
      return !m_last_error.has_value();
    }

   private:
    void setup_shellcode() noexcept {
      m_shellcode.write<std::uint32_t>(6, m_service_number);
      m_shellcode.setup();
    }

    std::optional<uint32_t> resolve_service_number() {
#ifndef SHADOWSYSCALLS_DISABLE_CACHING
      auto cached_ssn = detail::ssn_cache[m_name_hash];
      if (cached_ssn != 0)
        return cached_ssn;
#endif
      auto mod_export = exported_symbol(m_name_hash);
      if (mod_export == 0) {
        set_last_error(error::export_not_found);
        return std::nullopt;
      }

      auto parsed_ssn = m_ssn_parser(*this, mod_export.address());
#ifndef SHADOWSYSCALLS_DISABLE_CACHING
      if (parsed_ssn)
        detail::ssn_cache.try_emplace(m_name_hash, *parsed_ssn);
#endif
      return parsed_ssn;
    }

    // Syscall ID is at an offset of 4 bytes from the specified address.
    // \note: Not considering the situation when EDR hook is installed
    // Learn more here: https://github.com/annihilatorq/shadow_syscall/issues/1
    std::uint32_t default_ssn_parser(address_t export_address) {
      auto address = export_address.ptr<std::uint8_t>();
      for (auto i = 0; i < 24; ++i) {
        if (address[i] == 0x4c && address[i + 1] == 0x8b && address[i + 2] == 0xd1 &&
            address[i + 3] == 0xb8 && address[i + 6] == 0x00 && address[i + 7] == 0x00) {
          return *reinterpret_cast<std::uint32_t*>(&address[i + 4]);
        }
      }
      set_last_error(error::ssn_not_found);
      return 0;
    }

   private:
    hash64_t::underlying_t m_name_hash;
    std::uint32_t m_service_number;
    std::optional<std::uint32_t> m_last_error;
    ssn_parser_t m_ssn_parser;

    shellcode<13> m_shellcode = {
        0x49, 0x89, 0xCA,                          // mov r10, rcx
        0x48, 0xC7, 0xC0, 0x3F, 0x10, 0x00, 0x00,  // mov rax, {ssn}
        0x0F, 0x05,                                // syscall
        0xC3                                       // ret
    };
  };

  template <typename Ty, typename ResultTy = concepts::non_void_t<Ty>>
    requires(std::is_default_constructible_v<ResultTy>)
  class importer {
   public:
    explicit importer(hash64_t import_name, hash64_t module_name = 0)
        : m_export(get_export(import_name, module_name)) {}

    template <typename... Args>
    auto operator()(Args&&... args) noexcept {
      return m_export.address().execute<ResultTy>(
          shadow::detail::convert_nulls_to_nullptrs(args)...);
    }

    [[nodiscard]] auto export_location() const noexcept {
      return m_export.location();
    }

    [[nodiscard]] auto exported_symbol() const noexcept {
      return m_export;
    }

    friend std::ostream& operator<<(std::ostream& os, const importer& imp) {
      return os << imp.exported_symbol();
    }

   private:
    detail::exported_symbol get_export(hash64_t export_name, hash64_t module_name) {
#ifndef SHADOWSYSCALLS_DISABLE_CACHING
      detail::exported_symbol exp = detail::address_cache[export_name.get()];
      if (exp == 0) {
        exp = shadow::exported_symbol(export_name, module_name);
        detail::address_cache.try_emplace(export_name.get(), exp);
      }

      return exp;
#else
      return shadow::exported_symbol(export_name, module_name);
#endif
    }

    ResultTy m_call_result{};
    detail::exported_symbol m_export{0};
  };

  template <typename Ty = long, class... Args>
    requires(is_x64 && !concepts::function_type<Ty>)
  inline Ty shadowsyscall(hash64_t syscall_name, Args&&... args) {
    syscaller<Ty> sc{syscall_name};
    return sc(std::forward<Args>(args)...);
  }

  template <concepts::function_type F, class... Args,
            typename Traits = concepts::function_traits<F>>
    requires(is_x64 && concepts::args_compatible_v<F, Args...>)
  inline typename Traits::return_type shadowsyscall(hash64_t func_name, Args&&... args) {
    syscaller<typename Traits::return_type> sc{func_name};
    return sc(std::forward<Args>(args)...);
  }

  template <auto Func, class... Args, typename Traits = concepts::function_traits<decltype(Func)>>
    requires(is_x64 && concepts::args_compatible_v<decltype(Func), Args...>)
  inline typename Traits::return_type shadowsyscall(Args&&... args) {
    constexpr auto func_name = detail::extract_function_name<Func>();
    constexpr auto func_hash = hash64_t{func_name.data(), func_name.size()};
    syscaller<typename Traits::return_type> sc{func_hash};
    return sc(std::forward<Args>(args)...);
  }

  template <typename Ty = std::monostate, class... Args>
    requires(!concepts::function_type<Ty>)
  inline Ty shadowcall(hash64_t export_name, Args&&... args) {
    return importer<Ty>{export_name}(std::forward<Args>(args)...);
  }

  template <typename Ty = std::monostate, class... Args>
    requires(!concepts::function_type<Ty>)
  inline Ty shadowcall(hashpair export_and_module_names, Args&&... args) {
    const auto& [export_name, module_name] = export_and_module_names;
    return importer<Ty>{export_name, module_name}(std::forward<Args>(args)...);
  }

  template <concepts::function_type F, class... Args,
            typename Traits = concepts::function_traits<F>>
    requires(concepts::args_compatible_v<F, Args...>)
  inline typename Traits::return_type shadowcall(hash64_t function_name, Args&&... args) {
    if constexpr (Traits::is_void) {
      importer<typename Traits::return_type>{function_name}(std::forward<Args>(args)...);
    } else {
      return importer<typename Traits::return_type>{function_name}(std::forward<Args>(args)...);
    }
  }

  template <concepts::function_type F, class... Args,
            typename Traits = concepts::function_traits<F>>
    requires(concepts::args_compatible_v<F, Args...>)
  inline typename Traits::return_type shadowcall(hashpair export_and_module_names, Args&&... args) {
    const auto& [export_name, module_name] = export_and_module_names;
    if constexpr (Traits::is_void) {
      importer<typename Traits::return_type>{export_name, module_name}(std::forward<Args>(args)...);
    } else {
      return importer<typename Traits::return_type>{export_name,
                                                    module_name}(std::forward<Args>(args)...);
    }
  }

  template <auto Func, class... Args, typename Traits = concepts::function_traits<decltype(Func)>>
    requires(concepts::args_compatible_v<decltype(Func), Args...>)
  inline typename Traits::return_type shadowcall(Args&&... args) {
    constexpr auto func_name = detail::extract_function_name<Func>();
    constexpr auto func_hash = hash64_t{func_name.data(), func_name.size()};
    if constexpr (Traits::is_void) {
      importer<typename Traits::return_type>{func_hash}(std::forward<Args>(args)...);
    } else {
      return importer<typename Traits::return_type>{func_hash}(std::forward<Args>(args)...);
    }
  }

  template <auto Func, detail::fixed_string ModuleName, class... Args,
            typename Traits = concepts::function_traits<decltype(Func)>>
    requires(concepts::args_compatible_v<decltype(Func), Args...>)
  inline typename Traits::return_type shadowcall(Args&&... args) {
    constexpr auto func_name = detail::extract_function_name<Func>();
    constexpr auto func_hash = hash64_t{func_name.data(), func_name.size()};
    constexpr auto module_hash = hash64_t{ModuleName.value};
    if constexpr (Traits::is_void) {
      importer<typename Traits::return_type>{func_hash, module_hash}(std::forward<Args>(args)...);
    } else {
      return importer<typename Traits::return_type>{func_hash,
                                                    module_hash}(std::forward<Args>(args)...);
    }
  }

}  // namespace shadow

namespace std {
  template <>
  struct hash<shadow::address_t> : shadow::detail::type_hash<shadow::address_t> {};
  template <>
  struct hash<shadow::hash32_t> : shadow::detail::type_hash<shadow::hash32_t> {};
  template <>
  struct hash<shadow::hash64_t> : shadow::detail::type_hash<shadow::hash64_t> {};

  template <>
  struct formatter<shadow::address_t> : shadow::detail::type_format<shadow::address_t> {};
  template <>
  struct formatter<shadow::hash32_t> : shadow::detail::type_format<shadow::hash32_t> {};
  template <>
  struct formatter<shadow::hash64_t> : shadow::detail::type_format<shadow::hash64_t> {};
}  // namespace std

using shadow::shadowcall;
using shadow::shadowsyscall;

#endif
