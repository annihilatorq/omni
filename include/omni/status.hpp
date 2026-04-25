#pragma once

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
