#pragma once

#include <cstdint>

#include "omni/detail/config.hpp"
#include "omni/status.hpp"
#include "omni/win/object.hpp"

namespace omni::win {

  using nt_query_system_information_fn = omni::status(OMNI_NTAPI*)(std::uint32_t, void*, std::uint32_t, std::uint32_t*);
  using nt_open_process_fn = omni::status(OMNI_NTAPI*)(void**, std::uint32_t, object_attributes*, client_id*);

} // namespace omni::win
