set(SOURCE_FILES test_webcachesim.cpp)
add_executable(test_webcachesim ${SOURCE_FILES})
target_include_directories(test_webcachesim PUBLIC lib)
target_link_libraries(test_webcachesim webcachesim)
