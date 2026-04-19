#include "omni/hash.hpp"
#include "test_utils.hpp"

ut::suite<"omni::hash"> hash_suite = [] {
  using namespace omni::literals;

  "hash literals match explicitly constructed hash values"_test = [] {
    expect("hello omni"_fnv1a32 == omni::fnv1a32{"hello omni"});
    expect("hello omni"_fnv1a64 == omni::fnv1a64{"hello omni"});
    expect("hello omni"_hash == omni::default_hash{"hello omni"});

    expect(L"\uFF28\uFF49"_fnv1a32 == omni::fnv1a32{L"\uFF28\uFF49"});
    expect(L"\uFF28\uFF49"_fnv1a64 == omni::fnv1a64{L"\uFF28\uFF49"});
    expect(L"\uFF28\uFF49"_hash == omni::default_hash{L"\uFF28\uFF49"});
  };

  "fnv1a hashes match known reference values"_test = [] {
    expect("hello omni"_fnv1a32 == 0x8d8bf418);
    expect("kernel32"_fnv1a32 == 0xf66013f9);
    expect("sleep"_fnv1a32 == 0x89eabb08);

    expect("hello omni"_fnv1a64 == 0x3fce4b355136bdd8);
    expect("kernel32"_fnv1a64 == 0x28b0efc46c707979);
    expect("sleep"_fnv1a64 == 0x3d5dd56be3296048);
  };

  "fnv1a hashes are case-insensitive for narrow strings"_test = [] {
    expect("hello omni"_fnv1a32 == "hElLo OmNi"_fnv1a32);
    expect("ntyieldexecution"_fnv1a32 == "NtYieldExecution"_fnv1a32);
    expect("sleep"_fnv1a32 == "Sleep"_fnv1a32);

    expect("hello omni"_fnv1a64 == "hElLo OmNi"_fnv1a64);
    expect("ntyieldexecution"_fnv1a64 == "NtYieldExecution"_fnv1a64);
    expect("sleep"_fnv1a64 == "Sleep"_fnv1a64);
  };

  "hashing distinguishes strings with embedded nulls"_test = [] {
    constexpr std::string_view string_with_embedded_null{"ab\0cd", 5};
    constexpr std::string_view string_before_null{"ab", 2};

    expect(omni::fnv1a32{string_with_embedded_null} != omni::fnv1a32{string_before_null});
    expect(omni::fnv1a64{string_with_embedded_null} != omni::fnv1a64{string_before_null});
  };

  "fnv1a hashes are case-insensitive for wide ASCII strings"_test = [] {
    expect(L"Sleep"_fnv1a32 == L"sLeEp"_fnv1a32);
    expect(L"NtYieldExecution"_fnv1a32 == L"nTyIeLdExEcUtIoN"_fnv1a32);

    expect(L"Sleep"_fnv1a64 == L"sLeEp"_fnv1a64);
    expect(L"NtYieldExecution"_fnv1a64 == L"nTyIeLdExEcUtIoN"_fnv1a64);
  };

  "empty strings hash to the fnv1a offset basis"_test = [] {
    expect(""_fnv1a32 == 0x811c9dc5);
    expect(""_fnv1a64 == 0xcbf29ce484222325);

    expect(L""_fnv1a32 == 0x811c9dc5);
    expect(L""_fnv1a64 == 0xcbf29ce484222325);
  };

  "different input strings produce different hashes"_test = [] {
    expect("abc"_fnv1a32 != "abd"_fnv1a32);
    expect("abc"_fnv1a32 != "abcd"_fnv1a32);
    expect("abc"_fnv1a32 != "abc "_fnv1a32);

    expect("abc"_fnv1a64 != "abd"_fnv1a64);
    expect("abc"_fnv1a64 != "abcd"_fnv1a64);
    expect("abc"_fnv1a64 != "abc "_fnv1a64);
  };
};
