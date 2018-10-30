CMAKE_MINIMUM_REQUIRED(VERSION 3.5.1)

set(SOURCE_FILES webcachesim_cli.cpp ../include/simulation.h)
add_executable(webcachesim_cli ${SOURCE_FILES})
target_link_libraries(webcachesim_cli webcachesim)