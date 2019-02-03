set(SOURCE_FILES webcachesim_cli.cpp ../include/simulation.h)
add_executable(webcachesim_cli ${SOURCE_FILES})
target_link_libraries(webcachesim_cli webcachesim)

find_package(libmongocxx-static REQUIRED)
set(SOURCE_FILES webcachesim_cli_db.cpp ../include/simulation.h)
add_executable(webcachesim_cli_db ${SOURCE_FILES})

#message(STATUS ${LIBMONGOCXX_STATIC_INCLUDE_DIRS})

target_include_directories(webcachesim_cli_db
        PRIVATE ${LIBMONGOCXX_STATIC_INCLUDE_DIRS}
        )

target_link_libraries(webcachesim_cli_db
        PRIVATE ${LIBMONGOCXX_STATIC_LIBRARIES} webcachesim
        )

target_compile_definitions(webcachesim_cli_db
        PRIVATE ${LIBMONGOCXX_STATIC_DEFINITIONS}
        )
install(TARGETS webcachesim_cli_db DESTINATION bin)
