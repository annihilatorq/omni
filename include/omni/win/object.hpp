#pragma once

#include <cstdint>

namespace omni::win {

  struct client_id {
    void* unique_process;
    void* unique_thread;
  };

  struct object_attributes {
    std::uint32_t length;
    void* root_directory;
    void* object_name;
    std::uint32_t attributes;
    void* security_descriptor;
    void* security_quality_of_service;
  };

  static_assert(sizeof(client_id) == sizeof(void*) * 2);
  static_assert(sizeof(object_attributes) == (sizeof(void*) == 8 ? 0x30 : 0x18));

} // namespace omni::win
