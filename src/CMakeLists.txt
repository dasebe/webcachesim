cmake_minimum_required(VERSION 3.5.1)


set(LIB_SOURCE
        caches/lru_variants.cpp
        caches/gd_variants.cpp
        random_helper.cpp
        simulation.cpp
)
#message(123)
add_library(webcachesim SHARED ${LIB_SOURCE})

target_include_directories(webcachesim PUBLIC include)
