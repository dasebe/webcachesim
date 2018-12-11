CMAKE_MINIMUM_REQUIRED(VERSION 3.5.1)

set(SOURCE_FILES test_webcachesim.cpp)
add_executable(test_webcachesim ${SOURCE_FILES})
target_include_directories(test_webcachesim PUBLIC lib)
target_link_libraries(test_webcachesim webcachesim)

#set(TEST_SOURCE_FILES tmp.cpp)
#add_executable(testing ${TEST_SOURCE_FILES})
#target_include_directories(testing PUBLIC ../lib/LightGBM/include)
#TARGET_LINK_LIBRARIES(testing LINK_PUBLIC ${Boost_LIBRARIES} _lightgbm)
