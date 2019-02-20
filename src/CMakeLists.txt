set(LIB_SOURCE
        caches/lru_variants.cpp
        caches/gd_variants.cpp
        caches/random_variants.cpp
        caches/belady.cpp
        caches/lhd_competitors/lhd_competitors.cpp
        caches/lhd/lhd_variants.cpp
        caches/lhd/lhd.cpp
        random_helper.cpp
        simulation.cpp
#        simulation_lfo.cpp
#        simulation_lfo2.cpp
        simulation_future.cpp
#        simulation_truncate.cpp
        caches/annotate.cpp
        caches/ucb.cpp
        caches/belady_sample.cpp
        caches/lruk_sample.cpp
        caches/lfu_sample.cpp
#        simulation_lr_belady.cpp
#        simulation_belady_static.cpp
#        simulation_bins.cpp
        caches/bins.cpp
        caches/belady_truncate.cpp
        caches/gbdt.cpp
        caches/hyperbolic.cpp caches/lecar.cpp)
#message(123)

find_package(Boost)
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

add_library(webcachesim SHARED ${LIB_SOURCE})

target_include_directories(webcachesim PUBLIC include)
target_include_directories(webcachesim PUBLIC ../lib/LightGBM/include)

TARGET_LINK_LIBRARIES(webcachesim LINK_PUBLIC ${Boost_LIBRARIES} _lightgbm)
