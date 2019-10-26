#disable compiling webcachesim_cli because infrequently usage
#set(SOURCE_FILES webcachesim_cli.cpp ../include/simulation.h)
#add_executable(webcachesim_cli ${SOURCE_FILES})
#target_link_libraries(webcachesim_cli webcachesim)

set(SOURCE_FILES webcachesim_cli.cpp ${WEBCACHESIM_HEADER_DIR}/webcachesim/simulation.h)
add_executable(webcachesim_cli ${SOURCE_FILES})

find_package(libmongocxx REQUIRED)
#message(STATUS ${LIBMONGOCXX_INCLUDE_DIRS})
target_include_directories(webcachesim_cli PRIVATE ${LIBMONGOCXX_INCLUDE_DIRS})
target_link_libraries(webcachesim_cli PRIVATE ${LIBMONGOCXX_LIBRARIES} webcachesim)

target_compile_definitions(webcachesim_cli PRIVATE ${LIBMONGOCXX_DEFINITIONS})
