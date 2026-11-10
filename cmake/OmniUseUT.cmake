include_guard(GLOBAL)

include(FetchContent)

function(omni_use_ut dependent_target)
  if(NOT TARGET ut::ut)
    set(UT_ENABLE_MODULES OFF)
    set(UT_COMPILE_TIME OFF)

    FetchContent_Declare(annihilatorq_ut
      GIT_REPOSITORY https://github.com/annihilatorq/ut
      GIT_TAG b123c3d4dcb6974e4203aaca665b2729bb3f3b52
      EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(annihilatorq_ut)
  endif()

  target_link_libraries(${dependent_target} PRIVATE ut::ut)
endfunction()
