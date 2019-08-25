cmake_minimum_required (VERSION 3.2.2)
project (webcachesim)
set (CMAKE_CXX_STANDARD 11)

#include(CheckCXXCompilerFlag)
#CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
#if(COMPILER_SUPPORTS_MARCH_NATIVE)
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
#endif()

#set this flag to get statistics of miss decouple
#add_definitions(-DMISS_DECOUPLE)
remove_definitions(-DMISS_DECOUPLE)

#set this flag to get statistics of cache sizes decouple
#add_definitions(-DCACHE_SIZE_DECOUPLE)
remove_definitions(-DCACHE_SIZE_DECOUPLE)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math")

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/caches)

add_subdirectory(src)
#add_subdirectory(test)
add_subdirectory(integration)
