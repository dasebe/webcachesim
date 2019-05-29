set(LIB_SOURCE
        caches/lru_variants.cpp
        ../include/caches/lru_variants.h
        ../include/caches/lru2.h
        ../include/caches/wlc.h
        caches/gd_variants.cpp
        caches/random_variants.cpp
        caches/belady.cpp
        caches/lhd_competitors/lhd_competitors.cpp
        caches/lhd/lhd_variants.cpp
        caches/lhd/lhd.cpp
        random_helper.cpp
        simulation.cpp
        simulation_lfo.cpp
#        simulation_lfo2.cpp
        simulation_future.cpp
        simulation_truncate.cpp
        caches/annotate.cpp
        caches/ucb.cpp
        caches/belady_sample.cpp
        caches/lfo.cpp
#        caches/lruk_sample.cpp
#        caches/lfu_sample.cpp
#        simulation_lr_belady.cpp
#        simulation_belady_static.cpp
#        simulation_bins.cpp
        caches/bins.cpp
        caches/belady_truncate.cpp
        caches/wlc.cpp
        caches/hyperbolic.cpp caches/lecar.cpp simulation_tinylfu.cpp caches/lr.cpp)
#message(123)

find_package(Boost)
INCLUDE_DIRECTORIES( ${Boost_INCLUDE_DIR} )

find_library(LIGHTGBM_LIB lib_lightgbm.so)
find_path(LIGHTGBM_PATH LightGBM)

add_library(webcachesim SHARED ${LIB_SOURCE})

target_include_directories(webcachesim PRIVATE ../lib)
target_include_directories(webcachesim PUBLIC include)
target_include_directories(webcachesim PUBLIC ${LIGHTGBM_PATH})

target_link_libraries(webcachesim LINK_PUBLIC ${Boost_LIBRARIES} ${LIGHTGBM_LIB})
