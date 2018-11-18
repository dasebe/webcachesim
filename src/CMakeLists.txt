cmake_minimum_required(VERSION 3.5.1)


set(LIB_SOURCE
        caches/lru_variants.cpp
        caches/gd_variants.cpp
        caches/random_variants.cpp
        caches/belady.cpp
        caches/lfo.cpp
        random_helper.cpp
        simulation.cpp
        simulation_lfo.cpp
        simulation_belady.cpp
        simulation_lr.cpp
        caches/annotate.cpp)
#message(123)

find_package(Boost)
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

#add_library(lightgbm SHARED)


add_library(webcachesim SHARED ${LIB_SOURCE})

target_include_directories(webcachesim PUBLIC include)
target_include_directories(webcachesim PUBLIC LightGBM)

TARGET_LINK_LIBRARIES(webcachesim LINK_PUBLIC ${Boost_LIBRARIES} _lightgbm)
