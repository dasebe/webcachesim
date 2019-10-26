set(LIB_SOURCE
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/simulation.h
        simulation.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/simulation_lfo.h
        simulation_lfo.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/simulation_tinylfu.h
        simulation_tinylfu.cpp

        ${WEBCACHESIM_HEADER_DIR}/webcachesim/annotate.h
        caches/annotate.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/rss.h
        rss.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/bloom_filter.h
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/random_helper.h
        random_helper.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/bloom_filter.h

        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/lru_variants.h
        caches/lru_variants.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/belady.h
        caches/belady.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/lru2.h
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/gd_variants.h
        caches/gd_variants.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/random_variants.h
        caches/random_variants.cpp
        caches/lhd_competitors/lhd_competitors.h
        caches/lhd_competitors/lhd_competitors.cpp
        caches/lhd/lhd_variants.h
        caches/lhd/lhd_variants.cpp
        caches/lhd/lhd.hpp
        caches/lhd/lhd.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/ucb.h
        caches/ucb.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/belady_sample.h
        caches/belady_sample.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/wlc.h
        caches/wlc.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/hyperbolic.h
        caches/hyperbolic.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/lecar.h
        caches/lecar.cpp
        ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches/lr.h
        caches/lr.cpp
        #        caches/lfo.cpp
        #        caches/lruk_sample.cpp
        #        caches/lfu_sample.cpp
        #        simulation_lr_belady.cpp
        #        simulation_belady_static.cpp
        #        caches/belady_truncate.cpp
        #        ${WEBCACHESIM_HEADER_DIR}/simulation_truncate.h
        #        simulation_truncate.cpp
        )
#message(123)


add_library(webcachesim SHARED ${LIB_SOURCE})
target_include_directories(webcachesim PUBLIC ${WEBCACHESIM_HEADER_DIR}/webcachesim)
target_include_directories(webcachesim PUBLIC ${WEBCACHESIM_HEADER_DIR}/webcachesim/caches)
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

install(TARGETS webcachesim
        RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        )

install(DIRECTORY ${WEBCACHESIM_HEADER_DIR}/webcachesim DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
