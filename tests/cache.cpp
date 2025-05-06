#include "gtest/gtest.h"
#include "shadowsyscall.hpp"

TEST(cache, basics) {
  auto& ssn_cache = shadow::detail::ssn_cache;

  constexpr auto key = shadow::hash64_t{"Hello"}.get();
  constexpr std::uint32_t value = 1;
  ssn_cache.emplace(key, value);

  EXPECT_TRUE(ssn_cache.exists(key));
  EXPECT_EQ(ssn_cache.size(), 1);
  EXPECT_FALSE(ssn_cache.try_emplace(key, value));

  auto cached_value = ssn_cache[key];
  EXPECT_EQ(cached_value, value);

  ssn_cache.erase(key);

  EXPECT_FALSE(ssn_cache.exists(key));
  EXPECT_EQ(ssn_cache.size(), 0);
  EXPECT_TRUE(ssn_cache.try_emplace(key, value));
}