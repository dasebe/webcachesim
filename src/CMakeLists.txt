set(LIB_SOURCE
        caches/lru_variants.cpp
        ../include/caches/lru_variants.h
        ../include/caches/lru2.h
        caches/gd_variants.cpp
        caches/random_variants.cpp
        ../include/caches/belady.h
        caches/belady.cpp
        caches/lhd_competitors/lhd_competitors.cpp
        caches/lhd/lhd_variants.cpp
        caches/lhd/lhd.cpp
        random_helper.cpp
        ../include/simulation.h
        simulation.cpp
        ../include/simulation_lfo.h
        simulation_lfo.cpp
        ../include/simulation_truncate.h
        simulation_truncate.cpp
        caches/annotate.cpp
        caches/ucb.cpp
        caches/belady_sample.cpp
        #        caches/lfo.cpp
        #        caches/lruk_sample.cpp
        #        caches/lfu_sample.cpp
        #        simulation_lr_belady.cpp
        #        simulation_belady_static.cpp
        #        caches/belady_truncate.cpp
        ../include/caches/wlc.h
        caches/wlc.cpp
        caches/hyperbolic.cpp caches/lecar.cpp
        simulation_tinylfu.cpp ../include/simulation_tinylfu.h
        caches/lr.cpp
        rss.cpp ../include/rss.h
        ../include/bloom_filter.h)
#message(123)


add_library(webcachesim SHARED ${LIB_SOURCE})
target_include_directories(webcachesim PUBLIC include)
target_include_directories(webcachesim PUBLIC ../lib)

find_package(libmongocxx REQUIRED)
target_include_directories(webcachesim PRIVATE ${LIBMONGOCXX_INCLUDE_DIRS})
target_link_libraries(webcachesim PRIVATE ${LIBMONGOCXX_LIBRARIES})
target_compile_definitions(webcachesim PRIVATE ${LIBMONGOCXX_DEFINITIONS})

find_package(Boost REQUIRED)
target_include_directories(webcachesim PRIVATE ${Boost_INCLUDE_DIR})
target_link_libraries(webcachesim LINK_PUBLIC ${Boost_LIBRARIES})

find_path(LIGHTGBM_PATH LightGBM)
if (NOT LIGHTGBM_PATH)
    message(FATAL_ERROR "LIGHTGBM_PATH not found")
endif ()
target_include_directories(webcachesim PUBLIC ${LIGHTGBM_PATH})

find_library(LIGHTGBM_LIB lib_lightgbm.so)
if (NOT LIGHTGBM_LIB)
    message(FATAL_ERROR "LIGHTGBM_LIB not found")
endif ()
target_link_libraries(webcachesim LINK_PUBLIC ${LIGHTGBM_LIB})

find_library(PROCPS_LIB procps)
find_path(PROCPS_PATH proc)
target_include_directories(webcachesim PRIVATE ${PROCPS_PATH})
target_link_libraries(webcachesim PRIVATE ${PROCPS_LIB})
