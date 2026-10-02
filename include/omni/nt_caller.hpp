#pragma once

#include "omni/detail/config.hpp"
#include "omni/lazy_import.hpp"
#include "omni/status.hpp"
#include "omni/syscall.hpp"

namespace omni {

#ifdef OMNI_ARCH_X64
  template <typename T = omni::status>
  using default_nt_caller = omni::default_syscaller<T>;
#else
  template <typename T = omni::status>
  using default_nt_caller = omni::lazy_importer<T>;
#endif

} // namespace omni
