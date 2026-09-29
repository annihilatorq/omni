#pragma once

#include "omni/detail/config.hpp"
#include "omni/status.hpp"

#ifdef OMNI_ARCH_X64
#  include "omni/syscall.hpp"
#else
#  include "omni/lazy_import.hpp"
#endif

namespace omni {

#ifdef OMNI_ARCH_X64
  template <typename T = omni::status>
  using default_nt_caller = omni::default_syscaller<T>;
#else
  template <typename T = omni::status>
  using default_nt_caller = omni::lazy_importer<T>;
#endif

} // namespace omni
