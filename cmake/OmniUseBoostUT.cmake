include_guard(GLOBAL)

include(FetchContent)

function(_omni_try_enable_boost_ut dependent_target)
  if(NOT TARGET Boost::ut)
    find_package(ut QUIET)
  endif()

  if(TARGET Boost::ut)
    message(STATUS "[OMNI] Using boost.ut (Boost::ut)")
  endif()
endfunction()

function(_omni_enable_bundled_boost_ut dependent_target)
  set(boost_ut_source_dir "${CMAKE_BINARY_DIR}/_deps/boost_ut-src")
  set(boost_ut_binary_dir "${CMAKE_BINARY_DIR}/_deps/boost_ut-build")

  if(EXISTS "${boost_ut_source_dir}/CMakeLists.txt")
    message(STATUS "[OMNI] Using cached boost.ut source at ${boost_ut_source_dir}")
    add_subdirectory("${boost_ut_source_dir}" "${boost_ut_binary_dir}")
    return()
  endif()

  message(STATUS "[OMNI] Fetching boost.ut v2.3.1 from git...")

  FetchContent_Declare(boost_ut
    GIT_REPOSITORY  https://github.com/boost-ext/ut
    GIT_TAG         v2.3.1
    GIT_SHALLOW     ON
    UPDATE_DISCONNECTED ON
  )
  FetchContent_MakeAvailable(boost_ut)
endfunction()

function(omni_use_boost_ut dependent_target)
  if(NOT TARGET Boost::ut)
    _omni_try_enable_boost_ut(${dependent_target})

    if(NOT TARGET Boost::ut)
      _omni_enable_bundled_boost_ut(${dependent_target})
    endif()
  endif()

  target_link_libraries(${dependent_target} PRIVATE Boost::ut)
endfunction()
