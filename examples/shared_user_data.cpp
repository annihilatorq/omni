#include "omni/shared_user_data.hpp"

#include <print>

int main() {
  const auto* shared = omni::shared_user_data();

  std::println("KUSER_SHARED_DATA is mapped once and readable from every process:");
  std::println("  address              : {:#x}", reinterpret_cast<std::uintptr_t>(shared));
  std::println("  system root          : {}", shared->system_root().string());
  std::println("  NT version           : {}.{} build {}",
    shared->nt_major_version,
    shared->nt_minor_version,
    shared->nt_build_number);
  std::println("  active processors    : {}", shared->active_processor_count);
  std::println("  active groups        : {}", shared->active_group_count);
  std::println("  QPC frequency        : {}", shared->qpc_frequency);
  std::println("  safe boot            : {}", static_cast<bool>(shared->safe_boot_mode));
  std::println("  secure boot enabled  : {}", static_cast<bool>(shared->dbg_secure_boot_enabled));
  std::println("  virtualization flags : 0x{:02X}", shared->virtualization_flags);
}
