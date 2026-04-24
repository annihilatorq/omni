cmake_minimum_required(VERSION 3.21)

list(APPEND CMAKE_MODULE_PATH 
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake"
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/scripts"
)

include(Prelude)

project(omni_cpp_library LANGUAGES C CXX)

include(OmniOptions)

if (PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
	set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
endif()

add_library(omni INTERFACE)
add_library(omni::omni ALIAS omni)

target_include_directories(omni INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(omni INTERFACE cxx_std_23)

if(OMNI_DISABLE_EXCEPTIONS)
  target_compile_definitions(omni INTERFACE OMNI_DISABLE_EXCEPTIONS)

  if(MSVC)
    target_compile_options(omni INTERFACE /EHs-c-)
    target_compile_definitions(omni INTERFACE _HAS_EXCEPTIONS=0)
  else()
    target_compile_options(omni INTERFACE -fno-exceptions)
  endif()
endif()

if(OMNI_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(OMNI_BUILD_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif()
