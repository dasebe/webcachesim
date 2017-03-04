cmake_minimum_required (VERSION 2.9)

project (webcachesim)

add_compile_options(-std=c++11)
add_definitions(-O3 -Wall)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.8)
        message(FATAL_ERROR "We need GCC version > 4.8 for C++11")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 3.2)
        message(FATAL_ERROR "We need Clang version > 3.2 for C++11")
endif()

add_executable(webcachesim webcachesim.cc)

