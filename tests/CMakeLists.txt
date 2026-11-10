if(CMAKE_VERSION VERSION_LESS 3.31)
  message(FATAL_ERROR "[OMNI] Building tests requires CMake 3.31 or newer (found ${CMAKE_VERSION})")
endif()

set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

include(OmniUseUT)

message(STATUS "[OMNI] Configuring tests")

file(GLOB_RECURSE omni_unit_test_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

list(SORT omni_unit_test_sources)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(STATUS "[OMNI] Skipping syscall tests on x86")
  list(REMOVE_ITEM omni_unit_test_sources "${CMAKE_CURRENT_LIST_DIR}/syscall.cpp")
endif()

if(NOT OMNI_SUPPORTS_INLINE_SYSCALL)
  message(STATUS "[OMNI] Skipping inline syscall tests without OMNI_HAS_INLINE_SYSCALL")
  list(REMOVE_ITEM omni_unit_test_sources "${CMAKE_CURRENT_LIST_DIR}/inline_syscall.cpp")
endif()

function(omni_add_unit_test source)
  get_filename_component(test_name "${source}" NAME_WE)
  set(test_target "omni_test_${test_name}")

  add_executable("${test_target}" "${source}")
  omni_use_ut("${test_target}")

  target_compile_features("${test_target}" PRIVATE cxx_std_23)
  target_include_directories("${test_target}" PRIVATE ${omni_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries("${test_target}" PRIVATE omni::omni)

  add_test(NAME "${test_target}" COMMAND "${test_target}")
endfunction()

foreach(source IN LISTS omni_unit_test_sources)
  omni_add_unit_test("${source}")
endforeach()
