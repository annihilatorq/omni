#pragma once

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
