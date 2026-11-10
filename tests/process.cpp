#include <Windows.h>
#include <psapi.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "omni/process.hpp"
#include "ut/ut.hpp"

using namespace ut;
using omni::process_access;

namespace {

  // NOLINTNEXTLINE
  static_assert((process_access::vm_read | process_access::vm_read) == process_access::vm_read);
  static_assert(((process_access::vm_read | process_access{0x1}) & process_access::vm_read) == process_access::vm_read);
  static_assert(std::to_underlying(process_access::vm_read | process_access{0x1}) == 0x0011U);

  [[nodiscard]] std::optional<omni::process> find_process(const omni::processes& snapshot, std::uint32_t pid) noexcept {
    const auto it = std::ranges::find_if(snapshot, [pid](const omni::process& p) { return p.id() == pid; });
    if (it == snapshot.end()) {
      return std::nullopt;
    }
    return *it;
  }

  [[nodiscard]] std::wstring_view current_executable_name(std::span<wchar_t> storage) noexcept {
    const DWORD length = ::GetModuleFileNameW(nullptr, storage.data(), static_cast<DWORD>(storage.size()));
    const std::wstring_view path{storage.data(), length};
    return path.substr(path.find_last_of(L'\\') + 1);
  }

  [[nodiscard]] std::uint32_t current_session_id() noexcept {
    DWORD session{};
    ::ProcessIdToSessionId(::GetCurrentProcessId(), &session);
    return session;
  }

  [[nodiscard]] DWORD current_handle_count() noexcept {
    DWORD count{};
    ::GetProcessHandleCount(::GetCurrentProcess(), &count);
    return count;
  }

  [[nodiscard]] std::size_t current_private_bytes() noexcept {
    PROCESS_MEMORY_COUNTERS_EX counters{};
    ::GetProcessMemoryInfo(::GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters));
    return counters.PrivateUsage;
  }

  class suspended_child {
   public:
    suspended_child() noexcept {
      wchar_t command[] = L"cmd.exe";
      STARTUPINFOW startup{.cb = sizeof(STARTUPINFOW)};
      ::CreateProcessW(nullptr,
        reinterpret_cast<wchar_t*>(command),
        nullptr,
        nullptr,
        FALSE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &info_);
    }

    ~suspended_child() {
      if (info_.hProcess == nullptr) {
        return;
      }
      ::TerminateProcess(info_.hProcess, 0);
      ::CloseHandle(info_.hThread);
      ::CloseHandle(info_.hProcess);
    }

    suspended_child(const suspended_child&) = delete;
    suspended_child& operator=(const suspended_child&) = delete;

    [[nodiscard]] bool started() const noexcept {
      return info_.hProcess != nullptr;
    }
    [[nodiscard]] std::uint32_t id() const noexcept {
      return info_.dwProcessId;
    }

   private:
    PROCESS_INFORMATION info_{};
  };

} // namespace

int main() {
  ut::suite process_suite = [] {
    "snapshot enumerates the current process"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      const auto current = find_process(*snapshot, ::GetCurrentProcessId());
      require(current.has_value());
      expect(current->parent_id() != 0U);
      expect(current->thread_count() != 0U);
      expect(current->open_handle().has_value());
    };

    "first entry is Idle with pid 0 and empty name"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      const omni::process idle = *snapshot->begin();
      expect(idle.id() == 0U) << "first SystemProcessInformation iteration must return Idle process";
      expect(idle.name().empty()) << "Idle has a null image_name buffer, view() must not dereference it";
    };

    "snapshot contains System with pid 4"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      const auto system = find_process(*snapshot, 4U);
      require(system.has_value());
      expect(system->name() == L"System");
    };

    "current process name matches executable file name"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      wchar_t storage[MAX_PATH];
      const std::wstring_view expected = current_executable_name(storage);

      const auto current = find_process(*snapshot, ::GetCurrentProcessId());
      require(current.has_value());
      expect(current->name() == expected) << "view() length must be half of image_name byte-length";
    };

    "child process reports current process as parent"_test = [] {
      const suspended_child child;
      require(child.started());

      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      const auto entry = find_process(*snapshot, child.id());
      require(entry.has_value());
      expect(entry->parent_id() == ::GetCurrentProcessId());
      expect(entry->session_id() == current_session_id());
      expect(entry->thread_count() >= 1U) << "suspended process still has its initial thread";
    };

    "open_handle refers to the enumerated process"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      const auto current = find_process(*snapshot, ::GetCurrentProcessId());
      require(current.has_value());

      auto handle = current->open_handle();
      require(handle.has_value());
      expect(::GetProcessId(handle->get()) == current->id()) << "CLIENT_ID or access mask was built wrong";
    };

    "denies PROCESS_ALL_ACCESS to System without SeDebugPrivilege"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      const auto system = find_process(*snapshot, 4U);
      require(system.has_value());

      auto handle = system->open_handle(static_cast<omni::process_access>(PROCESS_ALL_ACCESS));
      require(!handle.has_value());
      expect(handle.error() == std::errc::permission_denied) << "NTSTATUS must map onto a portable error_condition";
    };

    "pids are unique within a snapshot"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      std::vector<std::uint32_t> pids;
      for (const omni::process& process : *snapshot) {
        pids.push_back(process.id());
      }
      std::ranges::sort(pids);
      expect(std::ranges::adjacent_find(pids) == pids.end()) << "pids must be unique within a snapshot";
    };

    "names contain no path separators"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      for (const omni::process& process : *snapshot) {
        expect(!process.name().contains(L'\\')) << "SystemProcessInformation returns names after the last backslash";
      }
    };

    "begin returns equal iterators on each call"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      expect(snapshot->begin() == snapshot->begin());
      expect(snapshot->begin() != snapshot->end());
    };

    "two passes enumerate the same pids"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      const auto ids = std::views::transform([](const omni::process& p) { return p.id(); });
      expect(std::ranges::equal(*snapshot | ids, *snapshot | ids)) << "forward range must be multi-pass";
      expect(std::ranges::distance(*snapshot) == std::ranges::distance(*snapshot)); // NOLINT
    };

    "works with views::filter and ranges::find_if"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      const std::uint32_t session = current_session_id();
      auto same_session =
        *snapshot | std::views::filter([session](const omni::process& p) { return p.session_id() == session; });
      expect(std::ranges::distance(same_session) >= 1);

      const auto self =
        std::ranges::find_if(*snapshot, [](const omni::process& p) { return p.id() == ::GetCurrentProcessId(); });
      expect(self != snapshot->end());
    };

    "moved-from snapshot is empty and moved-to iterates"_test = [] {
      auto source = omni::processes::snapshot();
      require(source.has_value());

      omni::processes target = std::move(*source);
      expect(source->begin() == source->end());
      expect(target.begin() != target.end());
      expect(find_process(target, ::GetCurrentProcessId()).has_value());
    };

    "default-constructed iterator equals end"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      expect(omni::processes::iterator{} == snapshot->end());
    };

    "post-increment returns the previous position"_test = [] {
      auto snapshot = omni::processes::snapshot();
      require(snapshot.has_value());

      auto it = snapshot->begin();
      const auto previous = it++;
      expect(previous == snapshot->begin());
      expect(it != snapshot->begin());
      expect((*previous).id() == 0U);
    };

    "repeated snapshots do not leak handles or memory"_test = [] {
      constexpr int warmup_iterations = 10;
      constexpr int measured_iterations = 1000;
      constexpr std::size_t memory_tolerance = 1U << 20U;

      const auto take_snapshot_and_open_self = [] {
        auto snapshot = omni::processes::snapshot();
        require(snapshot.has_value());
        const auto current = find_process(*snapshot, ::GetCurrentProcessId());
        require(current.has_value());
        expect(current->open_handle().has_value());
      };

      for (int i = 0; i < warmup_iterations; ++i) {
        take_snapshot_and_open_self();
      }

      const DWORD handles_before = current_handle_count();
      const std::size_t bytes_before = current_private_bytes();

      for (int i = 0; i < measured_iterations; ++i) {
        take_snapshot_and_open_self();
      }

      expect(current_handle_count() == handles_before) << "unique_handle or NtOpenProcess must not leak a handle per call";
      expect(current_private_bytes() <= bytes_before + memory_tolerance) << "virtual_free must release the buffer";
    };
  };
}
