#define _CRT_SECURE_NO_WARNINGS
#include "shadowsyscall.hpp"
#include "gtest/gtest.h"

#include <windows.h>

#include <cstring>
#include <string>

TEST( forwarded_exports, basics ) {
    HANDLE hHeap = shadowcall<HANDLE>( "GetProcessHeap" );
    ASSERT_NE( hHeap, nullptr ) << "GetProcessHeap failed with error " << GetLastError();

    // Allocate 256 bytes from using HeapAlloc (forwarded function)
    constexpr size_t initialSize = 256;
    LPVOID pMemory = shadowcall<LPVOID>( "HeapAlloc", hHeap, HEAP_ZERO_MEMORY, initialSize );
    ASSERT_NE( pMemory, nullptr ) << "HeapAlloc failed with error " << GetLastError();

    const char* message = "Hello, forwarded HeapAlloc!";
    std::strcpy( reinterpret_cast<char*>( pMemory ), message );
    EXPECT_STREQ( reinterpret_cast<const char*>( pMemory ), message );

    // Reallocate memory using HeapReAlloc (forwarded function)
    constexpr size_t newSize = 512;
    LPVOID pReallocMemory = shadowcall<LPVOID>( "HeapReAlloc", hHeap, HEAP_ZERO_MEMORY, pMemory, newSize );
    ASSERT_NE( pReallocMemory, nullptr ) << "HeapReAlloc failed with error " << GetLastError();
    pMemory = pReallocMemory;

    std::string expected = std::string( message );
    EXPECT_STREQ( reinterpret_cast<const char*>( pMemory ), expected.c_str() );

    auto freeResult = shadowcall<BOOL>( "HeapFree", hHeap, 0, pMemory );
    EXPECT_NE( freeResult.result(), 0 ) << "HeapFree failed with error " << GetLastError();
}
