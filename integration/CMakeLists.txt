set(SOURCE_FILES webcachesim_cli.cpp ${WEBCACHESIM_HEADER_DIR}/simulation.h)
add_executable(webcachesim_cli ${SOURCE_FILES})

find_package(libmongocxx REQUIRED)
target_include_directories(webcachesim_cli PRIVATE ${LIBMONGOCXX_INCLUDE_DIRS})
target_link_libraries(webcachesim_cli PRIVATE ${LIBMONGOCXX_LIBRARIES} webcachesim)

target_compile_definitions(webcachesim_cli PRIVATE ${LIBMONGOCXX_DEFINITIONS})
