#include "gtest/gtest.h"
#include "shadowsyscall.hpp"

using shadow::detail::basic_hash;
using shadow::detail::hasher_opts;

template <hasher_opts<uint64_t> Opts>
using hash64_t = basic_hash<uint64_t, Opts>;

TEST(hasher, basics) {
  constexpr std::string_view lowercase_string = "my string";
  constexpr std::string_view mixed_case_string = "My String";

  constexpr auto default_opts = hasher_opts{.seed = false};
  constexpr auto custom_seed_opts = hasher_opts<uint64_t>{.custom_seed = 100};
  constexpr auto case_sensitive_opts = hasher_opts{.case_sensitive = true, .seed = false};

  EXPECT_EQ(hash64_t<default_opts>{lowercase_string}, 1704693461258343940ULL);
  EXPECT_NE(hash64_t<custom_seed_opts>{lowercase_string}, 1704693461258343940ULL);
  EXPECT_EQ(hash64_t<case_sensitive_opts>{mixed_case_string}, 9354639888599838980ULL);
}
