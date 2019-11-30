cmake_minimum_required(VERSION 3.10)

project(WebCDN)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include_directories(caches)
include_directories(.)
include_directories("./lib/LightGBM/include")
include_directories("./lib")


#add_definitions(-ffast-math)
#add_definitions(-march=native)
#add_definitions(-mcmodel=medium)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ffast-math -march=native -mcmodel=medium")

set(SOURCES caches/lru_variants.cpp
            caches/gd_variants.cpp
            random_helper.cpp
            webcachesim.cpp caches/lfo_cache.cpp
            caches/lfo_cache.h
            caches/lfo_features.h
            caches/optimal.cpp
            caches/optimal.h
            lib/misc/hash_combine.h
            caches/solve_mcf.cpp
            caches/solve_mcf.h)

add_executable(bin ${SOURCES})

find_library(LIGHTGBM_LIB lib_lightgbm.dylib LightGBM)
target_link_libraries(bin ${LIGHTGBM_LIB})
