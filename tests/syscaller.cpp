#define SHADOWSYSCALLS_DISABLE_CACHING
#include "gtest/gtest.h"
#include "shadowsyscall.hpp"

using NTSTATUS = long;

constexpr uintptr_t invalid_handle = 0xFFFF;
constexpr NTSTATUS status_invalid_handle = 0xC0000008;
constexpr NTSTATUS status_success = 0;

TEST(syscaller, syscall_result) {
  shadow::syscaller<NTSTATUS> sc("NtClose");
  auto result = sc(invalid_handle);
  EXPECT_EQ(result, status_invalid_handle);
  ASSERT_TRUE(sc.last_error().has_value() == false);
}

TEST(syscaller, valid_ssn_parser) {
  auto valid_ssn_parser = [](shadow::syscaller<NTSTATUS>& instance,
                             shadow::address_t export_address) -> std::optional<uint32_t> {
    if (!export_address) {
      instance.set_last_error(shadow::error::ssn_not_found);
      return std::nullopt;
    }
    return *export_address.offset(4).ptr<std::uint32_t>();
  };

  shadow::syscaller<NTSTATUS> sc("NtQueryInformationProcess");
  sc.set_ssn_parser(valid_ssn_parser);

  auto current_process = reinterpret_cast<void*>(-1);
  std::uintptr_t debug_port{0};
  auto result = sc(current_process, 7, &debug_port, sizeof(std::uintptr_t), nullptr);

  EXPECT_EQ(result, status_success);
  ASSERT_FALSE(sc.last_error().has_value());
}

TEST(syscaller, invalid_ssn_parser) {
  auto invalid_ssn_parser = [](shadow::syscaller<NTSTATUS>& instance,
                               shadow::address_t) -> std::optional<uint32_t> {
    instance.set_last_error(shadow::error::ssn_not_found);
    return std::nullopt;
  };

  shadow::syscaller<NTSTATUS> sc("NtQueryInformationProcess");
  sc.set_ssn_parser(invalid_ssn_parser);

  auto current_process = reinterpret_cast<void*>(-1);
  std::uintptr_t debug_port{0};
  auto result = sc(current_process, 7, &debug_port, sizeof(std::uintptr_t), nullptr);

  EXPECT_EQ(result, -1);
  ASSERT_TRUE(sc.last_error().has_value() &&
              sc.last_error().value() == shadow::error::ssn_not_found);
}