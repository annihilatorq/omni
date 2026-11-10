#include <Windows.h>

#include <string>
#include <string_view>
#include <system_error>

#include "omni/error.hpp"
#include "ut/ut.hpp"

using namespace ut;

int main() {
  ut::suite error_suite = [] {
    "make_error_code keeps the NTSTATUS value and uses the nt category"_test = [] {
      const auto error = omni::make_error_code(omni::ntstatus::access_denied);

      expect(error.value() == omni::ntstatus::access_denied.value);
      expect(error.category() == omni::nt_error_category());
    };

    "success status converts to false"_test = [] {
      expect(not omni::make_error_code(omni::ntstatus::success));
    };

    "access_denied compares equal to errc::permission_denied"_test = [] {
      expect(omni::make_error_code(omni::ntstatus::access_denied) == std::errc::permission_denied)
        << "default_error_condition must go through RtlNtStatusToDosError";
    };

#ifdef OMNI_HAS_ERROR_STRINGS

    "category name is omni.nt_error when error strings are enabled"_test = [] {
      expect(std::string_view{omni::nt_error_category().name()} == "omni.nt_error");
    };

    "message matches the Win32 translation of the status"_test = [] {
      constexpr std::pair<omni::status, DWORD> translations[]{
        {omni::ntstatus::success, ERROR_SUCCESS},
        {omni::ntstatus::access_denied, ERROR_ACCESS_DENIED},
        {omni::ntstatus::invalid_handle, ERROR_INVALID_HANDLE},
      };

      for (const auto& [status, win32] : translations) {
        expect(omni::make_error_code(status).message() == std::system_category().message(static_cast<int>(win32)))
          << "NTSTATUS " << status.value;
      }
    };

#else

    "category name matches the system category when error strings are disabled"_test = [] {
      expect(std::string_view{omni::nt_error_category().name()} == std::system_category().name());
    };

    "message is empty when error strings are disabled"_test = [] {
      expect(omni::make_error_code(omni::ntstatus::access_denied).message().empty());
    };

#endif
  };
}
