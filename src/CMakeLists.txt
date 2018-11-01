cmake_minimum_required(VERSION 3.5.1)


set(LIB_SOURCE
        caches/lru_variants.cpp
        caches/gd_variants.cpp
        random_helper.cpp
        simulation.cpp
)
#message(123)

find_package(Boost)

INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )


add_library(webcachesim SHARED ${LIB_SOURCE})

target_include_directories(webcachesim PUBLIC include)

TARGET_LINK_LIBRARIES(webcachesim LINK_PUBLIC ${Boost_LIBRARIES} )
