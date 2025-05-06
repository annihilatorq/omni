#include "gtest/gtest.h"
#include "shadowsyscall.hpp"

constexpr auto test_address = 0x7ffe0000;

static_assert(
    std::is_same_v<decltype(std::declval<shadow::address_t>().ptr<uint32_t>()), uint32_t*>);

TEST(address, basics) {
  uint32_t* memory_ptr = reinterpret_cast<uint32_t*>(test_address);
  shadow::address_t address = test_address;

  EXPECT_EQ(address, test_address);
  EXPECT_EQ(address, memory_ptr);

  EXPECT_EQ(address.offset(4), test_address + 4);
  EXPECT_EQ(address.ptr<uint32_t>(4), memory_ptr + (4 / sizeof(uint32_t)));

  ASSERT_TRUE(address.is_in_range(test_address, test_address + 1));
  ASSERT_FALSE(address.is_in_range(test_address + 1, test_address));
  ASSERT_FALSE(address.is_in_range(test_address - 1, test_address));
  ASSERT_TRUE(address.is_in_range(test_address - 1, test_address + 1));
}

TEST(address, comparison_operators) {
  shadow::address_t address1(0x1000);
  shadow::address_t address2(0x2000);
  shadow::address_t address3(0x1000);

  EXPECT_EQ((address1 + address2).get(), 0x3000);
  EXPECT_EQ((address2 - address1).get(), 0x1000);

  EXPECT_TRUE(address1 < address2);
  EXPECT_TRUE(address2 > address1);
  EXPECT_TRUE(address1 <= address3);
  EXPECT_TRUE(address1 >= address3);

  EXPECT_TRUE(address1 == address3);
  EXPECT_TRUE(address1 != address2);

  EXPECT_EQ(static_cast<std::uintptr_t>(address1), 0x1000);
}

TEST(address, math_operators) {
  shadow::address_t address1(0x1000);
  shadow::address_t address2(0x2000);

  EXPECT_EQ((address1 + address2).get(), 0x3000);
  EXPECT_EQ((address2 - address1).get(), 0x1000);

  address1 += address2;
  EXPECT_EQ(address1.get(), 0x3000ULL);

  address1 -= address2;
  EXPECT_EQ(address1.get(), 0x1000ULL);
}

TEST(address, casts) {
  shadow::address_t address(0x1000);

  EXPECT_TRUE(static_cast<bool>(address));
  EXPECT_FALSE(static_cast<bool>(shadow::address_t(nullptr)));
  EXPECT_FALSE(static_cast<bool>(shadow::address_t(0)));
  EXPECT_EQ(static_cast<std::uintptr_t>(address), 0x1000);
}

TEST(address, views) {
  std::vector<char> vec{1, 2, 3, 4, 5};
  std::span<const char> vec_span{vec};

  shadow::address_t address(vec.data());
  auto span_from_address = address.span<const char>(vec.size());

  ASSERT_TRUE(std::ranges::equal(vec, span_from_address));
  ASSERT_TRUE(std::ranges::equal(vec_span, span_from_address));
}

TEST(address, constructors) {
  EXPECT_EQ(shadow::address_t(nullptr).get(), 0);
  EXPECT_EQ(shadow::address_t(0x1234).get(), 0x1234);

  std::vector<int> vec{1, 2, 3, 4, 5};
  std::array<int, 5> arr{1, 2, 3, 4, 5};
  std::string str{"string"};

  shadow::address_t{vec};
  shadow::address_t{arr};
  shadow::address_t{str};

  uint32_t value = 0;
  EXPECT_EQ(shadow::address_t(&value).get(),
            reinterpret_cast<shadow::address_t::underlying_t>(&value));
}