#define _CRT_SECURE_NO_WARNINGS
#include "gtest/gtest.h"
#include "shadowsyscall.hpp"

#include <windows.h>

#include <cstring>
#include <string>

TEST(forwarded_exports, basics) {
  HANDLE heap_handle = shadowcall<HANDLE>("GetProcessHeap");
  ASSERT_NE(heap_handle, nullptr) << "GetProcessHeap failed with error " << GetLastError();

  // Allocate 256 bytes from using HeapAlloc (forwarded function)
  constexpr size_t initial_size = 256;
  LPVOID memory_ptr = shadowcall<LPVOID>("HeapAlloc", heap_handle, HEAP_ZERO_MEMORY, initial_size);
  ASSERT_NE(memory_ptr, nullptr) << "HeapAlloc failed with error " << GetLastError();

  const char* message = "Hello, forwarded HeapAlloc!";
  std::strcpy(reinterpret_cast<char*>(memory_ptr), message);
  EXPECT_STREQ(reinterpret_cast<const char*>(memory_ptr), message);

  // Reallocate memory using HeapReAlloc (forwarded function)
  constexpr size_t new_size = 512;
  LPVOID pReallocMemory =
      shadowcall<LPVOID>("HeapReAlloc", heap_handle, HEAP_ZERO_MEMORY, memory_ptr, new_size);
  ASSERT_NE(pReallocMemory, nullptr) << "HeapReAlloc failed with error " << GetLastError();
  memory_ptr = pReallocMemory;

  std::string expected = std::string(message);
  EXPECT_STREQ(reinterpret_cast<const char*>(memory_ptr), expected.c_str());

  auto free_result = shadowcall<BOOL>("HeapFree", heap_handle, 0, memory_ptr);
  EXPECT_NE(free_result, 0) << "HeapFree failed with error " << GetLastError();
}
