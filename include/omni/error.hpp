#pragma once

#include <system_error>

#include "omni/detail/config.hpp"

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
