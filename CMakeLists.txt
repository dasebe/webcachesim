cmake_minimum_required (VERSION 3.5.1)
project (webcachesim)
set (CMAKE_CXX_STANDARD 11)

#set(CMAKE_BUILD_TYPE Debug CACHE STRING "build type (Debug/Release)")
#set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug;Release")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(/usr/include/python3.5m/)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/caches)

add_subdirectory(lib)
add_subdirectory(src)
add_subdirectory(test)
add_subdirectory(integration)
