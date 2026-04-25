if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(
      FATAL_ERROR
      "In-tree builds are not supported. "
      "Run CMake from a separate directory: cmake -B build "
      "You may need to delete 'CMakeCache.txt' and 'CMakeFiles/' first."
  )
endif()
