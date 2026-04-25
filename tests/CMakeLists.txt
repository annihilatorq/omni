include(OmniUseBoostUT)

message(STATUS "[OMNI] Configuring tests")

file(GLOB_RECURSE omni_unit_test_sources CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
)

list(SORT omni_unit_test_sources)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(STATUS "[OMNI] Skipping syscall tests on x86")
  list(REMOVE_ITEM omni_unit_test_sources "${CMAKE_CURRENT_LIST_DIR}/syscall.cpp")
endif()

set(omni_unit_test_main "${CMAKE_CURRENT_LIST_DIR}/main.cpp")
list(REMOVE_ITEM omni_unit_test_sources "${omni_unit_test_main}")

function(omni_add_unit_test source)
  get_filename_component(test_name "${source}" NAME_WE)
  set(test_target "omni_test_${test_name}")

  # boost.ut registers global suites during dynamic initialization.
  # Keep the runner TU first so suite registration order stays stable on Windows.
  add_executable("${test_target}" "${omni_unit_test_main}" "${source}")
  omni_use_boost_ut("${test_target}")

  target_compile_features("${test_target}" PRIVATE cxx_std_23)
  target_include_directories("${test_target}" PRIVATE ${omni_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
  target_link_libraries("${test_target}" PRIVATE omni::omni)

  add_test(NAME "${test_target}" COMMAND "${test_target}")
endfunction()

foreach(source IN LISTS omni_unit_test_sources)
  omni_add_unit_test("${source}")
endforeach()
