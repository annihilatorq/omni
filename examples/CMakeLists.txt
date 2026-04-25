message(STATUS "[OMNI] Configuring examples")

file(GLOB omni_example_sources CONFIGURE_DEPENDS *.cpp)
list(SORT omni_example_sources)

if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(STATUS "[OMNI] Skipping syscall example on x86")
  list(REMOVE_ITEM omni_example_sources "${CMAKE_CURRENT_LIST_DIR}/syscall.cpp")
endif()

foreach(source IN LISTS omni_example_sources)
  get_filename_component(example_name "${source}" NAME_WE)

  add_executable("${example_name}" "${source}")
  target_link_libraries("${example_name}" PRIVATE omni::omni)

  if(MINGW AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # MinGW/libstdc++ keeps std::print terminal support in stdc++exp.
    target_link_libraries("${example_name}" PRIVATE stdc++exp)
  endif()
endforeach()
