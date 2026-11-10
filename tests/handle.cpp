#include <Windows.h>

#include <utility>

#include "omni/handle.hpp"
#include "test_utils.hpp"

namespace {

  using nt_close_fn = omni::status(NTAPI*)(HANDLE);

  [[nodiscard]] HANDLE create_event_handle() {
    return ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
  }

  [[nodiscard]] nt_close_fn get_direct_nt_close() {
    HMODULE ntdll_handle = ::GetModuleHandleW(L"ntdll.dll");
    return reinterpret_cast<nt_close_fn>(::GetProcAddress(ntdll_handle, "NtClose"));
  }

  [[nodiscard]] bool is_open_os_handle(HANDLE handle) {
    if (handle == nullptr) {
      return false;
    }

    DWORD flags{};
    return ::GetHandleInformation(handle, &flags) != FALSE;
  }

  [[nodiscard]] bool is_closed_os_handle(HANDLE handle) {
    if (handle == nullptr) {
      return true;
    }

    DWORD flags{};
    ::SetLastError(ERROR_SUCCESS);
    return ::GetHandleInformation(handle, &flags) == FALSE && ::GetLastError() == ERROR_INVALID_HANDLE;
  }

  void cleanup_os_handle(HANDLE& handle) {
    if (is_open_os_handle(handle)) {
      ::CloseHandle(handle);
    }

    handle = nullptr;
  }

} // namespace

int main() {
  ut::suite handle_suite = [] {
    "detail::nt_close matches ntdll NtClose for invalid and valid handles"_test = [] {
      auto direct_nt_close = get_direct_nt_close();
      HANDLE wrapper_handle = create_event_handle();
      HANDLE direct_handle = create_event_handle();

      require(direct_nt_close != nullptr);
      require(wrapper_handle != nullptr);
      require(direct_handle != nullptr);

      const omni::status wrapper_invalid_status = omni::detail::nt_close(nullptr);
      const omni::status direct_invalid_status = direct_nt_close(nullptr);

      expect(wrapper_invalid_status == direct_invalid_status);

      const omni::status wrapper_valid_status = omni::detail::nt_close(wrapper_handle);
      const omni::status direct_valid_status = direct_nt_close(direct_handle);

      expect(wrapper_valid_status == direct_valid_status);
      expect(wrapper_valid_status == omni::ntstatus::success);
      expect(is_closed_os_handle(wrapper_handle));
      expect(is_closed_os_handle(direct_handle));
    };

    "detail::nt_close closes a kernel handle"_test = [] {
      HANDLE handle = create_event_handle();

      require(handle != nullptr);
      require(is_open_os_handle(handle));

      const omni::status status = omni::detail::nt_close(handle);

      expect(status == omni::ntstatus::success);
      expect(is_closed_os_handle(handle));
    };

    "default constructed unique_handle is empty"_test = [] {
      omni::unique_handle handle{};

      expect(handle.get() == nullptr);
      expect(not handle.valid());
      expect(not static_cast<bool>(handle));
      expect(handle.close() == omni::ntstatus::success);
      expect(handle.get() == nullptr);
    };

    "unique_handle reports ownership of a live kernel handle"_test = [] {
      HANDLE raw_handle = create_event_handle();
      omni::unique_handle handle{raw_handle};

      require(raw_handle != nullptr);

      expect(handle.get() == raw_handle);
      expect(handle.valid());
      expect(static_cast<bool>(handle));

      HANDLE released = handle.release();
      expect(released == raw_handle);
      cleanup_os_handle(released);
    };

    "release hands ownership to the caller without closing the handle"_test = [] {
      HANDLE raw_handle = create_event_handle();
      omni::unique_handle handle{raw_handle};

      require(raw_handle != nullptr);

      HANDLE released = handle.release();

      expect(released == raw_handle);
      expect(handle.get() == nullptr);
      expect(not handle.valid());
      expect(not static_cast<bool>(handle));
      expect(is_open_os_handle(released));

      cleanup_os_handle(released);
    };

    "out_ptr lets WinAPI output parameters populate empty wrappers"_test = [] {
      omni::unique_handle read_end;
      omni::unique_handle write_end;

      const BOOL created = ::CreatePipe(read_end.out_ptr(), write_end.out_ptr(), nullptr, 0U);

      require(created != FALSE);
      expect(read_end.valid());
      expect(write_end.valid());
      require(is_open_os_handle(static_cast<HANDLE>(read_end.get())));
      require(is_open_os_handle(static_cast<HANDLE>(write_end.get())));

      auto released_read_end = static_cast<HANDLE>(read_end.release());
      auto released_write_end = static_cast<HANDLE>(write_end.release());

      cleanup_os_handle(released_read_end);
      cleanup_os_handle(released_write_end);
    };

    "out_ptr closes the current handle before exposing storage for a replacement"_test = [] {
      HANDLE first = create_event_handle();
      HANDLE second = create_event_handle();
      omni::unique_handle handle{first};

      require(first != nullptr);
      require(second != nullptr);

      auto* out = handle.out_ptr();

      expect(out != nullptr);
      expect(handle.get() == nullptr);
      expect(not handle.valid());
      expect(is_closed_os_handle(first));

      *out = second;

      expect(handle.get() == second);
      expect(handle.valid());
      expect(is_open_os_handle(second));

      auto released = static_cast<HANDLE>(handle.release());
      cleanup_os_handle(released);
    };

    "out_ptr leaves the wrapper empty when no new handle is produced"_test = [] {
      HANDLE raw_handle = create_event_handle();
      omni::unique_handle handle{raw_handle};

      require(raw_handle != nullptr);

      auto* out = handle.out_ptr();

      expect(out != nullptr);
      expect(handle.get() == nullptr);
      expect(not handle.valid());
      expect(is_closed_os_handle(raw_handle));

      *out = nullptr;

      expect(handle.get() == nullptr);
      expect(not handle.valid());
      expect(handle.close() == omni::ntstatus::success);
    };

    "reset closes the previous handle and adopts the replacement"_test = [] {
      HANDLE first = create_event_handle();
      HANDLE second = create_event_handle();
      omni::unique_handle handle{first};

      require(first != nullptr);
      require(second != nullptr);

      handle.reset(second);

      expect(handle.get() == second);
      expect(handle.valid());
      expect(is_closed_os_handle(first));
      expect(is_open_os_handle(second));

      HANDLE released = handle.release();
      expect(released == second);
      cleanup_os_handle(released);
    };

    "reset without arguments closes the current handle and clears the wrapper"_test = [] {
      HANDLE raw_handle = create_event_handle();
      omni::unique_handle handle{raw_handle};

      require(raw_handle != nullptr);

      handle.reset();

      expect(handle.get() == nullptr);
      expect(not handle.valid());
      expect(not static_cast<bool>(handle));
      expect(is_closed_os_handle(raw_handle));
    };

    "close releases ownership only after the OS handle is actually closed"_test = [] {
      HANDLE raw_handle = create_event_handle();
      omni::unique_handle handle{raw_handle};

      require(raw_handle != nullptr);

      const omni::status status = handle.close();

      expect(status == omni::ntstatus::success);
      expect(handle.get() == nullptr);
      expect(not handle.valid());
      expect(not static_cast<bool>(handle));
      expect(is_closed_os_handle(raw_handle));
      expect(handle.close() == omni::ntstatus::success);
    };

    "move construction transfers ownership and empties the source"_test = [] {
      HANDLE raw_handle = create_event_handle();
      omni::unique_handle source{raw_handle};

      require(raw_handle != nullptr);

      omni::unique_handle moved{std::move(source)};

      expect(source.get() == nullptr);
      expect(not source.valid());
      expect(moved.get() == raw_handle);
      expect(moved.valid());

      HANDLE released = moved.release();
      expect(released == raw_handle);
      cleanup_os_handle(released);
    };

    "move assignment transfers ownership, closes the old target handle, and empties the source"_test = [] {
      HANDLE source_raw = create_event_handle();
      HANDLE target_raw = create_event_handle();
      omni::unique_handle source{source_raw};
      omni::unique_handle target{target_raw};

      require(source_raw != nullptr);
      require(target_raw != nullptr);

      target = std::move(source);

      expect(source.get() == nullptr);
      expect(not source.valid());
      expect(target.get() == source_raw);
      expect(target.valid());
      expect(is_closed_os_handle(target_raw));

      HANDLE released = target.release();
      expect(released == source_raw);
      cleanup_os_handle(released);
    };

    "swap exchanges ownership between wrappers"_test = [] {
      HANDLE first_raw = create_event_handle();
      HANDLE second_raw = create_event_handle();
      omni::unique_handle first{first_raw};
      omni::unique_handle second{second_raw};

      require(first_raw != nullptr);
      require(second_raw != nullptr);

      swap(first, second);

      expect(first.get() == second_raw);
      expect(second.get() == first_raw);

      HANDLE released_first = first.release();
      HANDLE released_second = second.release();

      expect(released_first == second_raw);
      expect(released_second == first_raw);

      cleanup_os_handle(released_first);
      cleanup_os_handle(released_second);
    };

    "destructor closes the owned handle"_test = [] {
      HANDLE raw_handle = create_event_handle();

      require(raw_handle != nullptr);

      {
        omni::unique_handle handle{raw_handle};
        expect(handle.get() == raw_handle);
        expect(handle.valid());
      }

      expect(is_closed_os_handle(raw_handle));
    };
  };
}
