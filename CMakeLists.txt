cmake_minimum_required(VERSION 3.17)

project(
    WCCOARAMS7200 
    VERSION 2.3.0
    DESCRIPTION "RAMS7200 driver for WinCC OA"
    LANGUAGES CXX
)

configure_file(config.h.in configured/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/configured)
set(DRV_VERSION ${PROJECT_VERSION})

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
else()
    # if Coverage is selected, look for required tools
    if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
        find_program(LCOV_PATH lcov)
        if(NOT LCOV_PATH)
            message(FATAL_ERROR "lcov not found! Cannot build coverage.")
        endif() 
    endif()
    # Code coverage report target
    add_custom_target(coverage
        COMMAND lcov --capture --directory . --output-file coverage.info
        COMMAND lcov --remove coverage.info '/usr/*' '*/test/*' '*/CMakeFiles/*' --output-file coverage.info.cleaned
        COMMAND genhtml coverage.info.cleaned --output-directory coverage
        COMMAND xdg-open coverage/index.html
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report..."
        USES_TERMINAL
    )
endif()

set(CMAKE_CXX_FLAGS "-rdynamic")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -ggdb")
set(CMAKE_CXX_FLAGS_COVERAGE "-O0 -g --coverage -fPIC")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")


# Define target
set(TARGET ${PROJECT_NAME})

# Include WinCC_OA API
set(API_ROOT "$ENV{API_ROOT}" CACHE FILEPATH "directory of the WinCC_OA API installation")
include(${API_ROOT}/CMakeDefines.txt)

# Collect sources
file(GLOB RAMS7200_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/RAMS7200*.cxx)
file(GLOB RAMS7200_TRANSFORMATIONS ${CMAKE_CURRENT_SOURCE_DIR}/Transformations/RAMS7200*.cxx)
file(GLOB RAMS7200_COMMON ${CMAKE_CURRENT_SOURCE_DIR}/Common/*.cxx)
set(S7_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/snap7.cpp)
set(SOURCES ${RAMS7200_SOURCES} ${RAMS7200_TRANSFORMATIONS} ${RAMS7200_COMMON} ${S7_SOURCES}) 

# Add driver
add_driver(${TARGET} ${SOURCES})

# Link snap7 library
find_library(snap7_lib snap7 PATHS ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${TARGET} ${snap7_lib})
# Copy the library to the build directory
add_custom_command(TARGET ${TARGET} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${snap7_lib} ${CMAKE_BINARY_DIR}
    COMMENT "Copying ${snap7_lib} to ${CMAKE_BINARY_DIR}"
)


# PVSS_PROJ_PATH Install 
# Check if PVSS_PROJ_PATH is set
if(NOT DEFINED ENV{PVSS_PROJ_PATH})
    message(WARNING "PVSS_PROJ_PATH environment variable is not set. Commodity targets will not be available (install, run, valgrind).")
else()
    # Install driver to PVSS_PROJ_PATH/bin
    install(TARGETS ${TARGET} DESTINATION $ENV{PVSS_PROJ_PATH}/bin)
    # Install snap7 library alongside the driver
    install(FILES ${snap7_lib} DESTINATION $ENV{PVSS_PROJ_PATH}/bin)

    # PVSS_PROJ_PATH contains the project name , so we can extract it
    string(REGEX MATCH "([^/]+)/?$" PVSS_PROJECT_NAME $ENV{PVSS_PROJ_PATH})
    string(REGEX REPLACE "/$" "" PVSS_PROJECT_NAME ${PVSS_PROJECT_NAME})
    message(STATUS "PVSS project name extracted from $PVSS_PROJ_PATH ($ENV{PVSS_PROJ_PATH}): ${PVSS_PROJECT_NAME}")

    # Try to parse config/progs file to extract driver number and its config file
    # i.e. WCCOARAMS7200    | always |      30 |        3 |        1 |-num 32 +config config.rams7200
    if(NOT EXISTS $ENV{PVSS_PROJ_PATH}/config/progs)
        message(WARNING "Cannot find $PVSS_PROJ_PATH/config/progs. Setting driver number to 999.")
        set(DRIVER_NUMBER 999)
    else()
        file(STRINGS $ENV{PVSS_PROJ_PATH}/config/progs PROGS)
        # Extract driver number and config file from config/progs
        if(EXISTS $ENV{PVSS_PROJ_PATH}/config/progs)
            file(STRINGS $ENV{PVSS_PROJ_PATH}/config/progs PROGS)
            string(REGEX MATCH "${TARGET}.*-num ([0-9]+) \\+config (config\\.[^ ^;]+)" DRIVER_MATCH "${PROGS}")
            if(DRIVER_MATCH)
                set(DRIVER_NUMBER ${CMAKE_MATCH_1})
                set(DRIVER_CONFIG_FILE ${CMAKE_MATCH_2})
                message(STATUS "Driver number extracted from $ENV{PVSS_PROJ_PATH}/config/progs: ${DRIVER_NUMBER}")
                message(STATUS "Driver config file extracted from $ENV{PVSS_PROJ_PATH}/config/progs: ${DRIVER_CONFIG_FILE}")
            endif()
        endif()
        if(NOT DEFINED DRIVER_NUMBER)
            message(WARNING "Driver number could not be extracted from $PVSS_PROJ_PATH/config/progs. Setting it to 999.")
            set(DRIVER_NUMBER 999)
        endif() 
    endif()
    
    # Run local build target (useful for debugging, coverage, valgrind, etc.)
    set(RUNBUILD_ARGS "-num ${DRIVER_NUMBER} -proj ${PVSS_PROJECT_NAME} +config $ENV{PVSS_PROJ_PATH}/config/${DRIVER_CONFIG_FILE} &")
    separate_arguments(RUNBUILD_ARGS)
    add_custom_target(run
        COMMAND ${TARGET} ${RUNBUILD_ARGS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Launching: ${TARGET} ${RUNBUILD_ARGS}"
        USES_TERMINAL
        COMMAND_EXPAND_LISTS
    )
    add_dependencies(run ${TARGET})

    # valgrind target
    find_program(VALGRIND_PATH valgrind)
    if(VALGRIND_PATH)
        set(VALGRIND_CMD ${VALGRIND_PATH} "--leak-check=full" "--show-leak-kinds=all" "--track-origins=yes" "--verbose" "--log-file=valgrind.log" "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}" ${RUNBUILD_ARGS})
        add_custom_target(valgrind
            COMMAND ${VALGRIND_CMD}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Launching: ${VALGRIND_CMD} \n\t Log file: ${CMAKE_BINARY_DIR}/valgrind.log. \n\t Use `kill` target to stop the process."
            USES_TERMINAL
            COMMAND_EXPAND_LISTS
        )
        add_dependencies(valgrind ${TARGET})
    endif()

endif()

# Send SIGTERM to the driver or valgrind process
add_custom_target(kill
    COMMAND pkill -SIGTERM -u "$ENV{USER}" -f "${TARGET}" -e
    COMMENT "Sending SIGTERM for ${TARGET}. WinCC OA will restart it automatically if configured to `always`."
    USES_TERMINAL
)

# Clean and update driver
add_custom_target(update
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target install
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target kill
)



# Config summary
message(STATUS     "")
message(STATUS     "---------------+-----------------------------------------------------------------------------------------------")
message(STATUS     "Configured ${TARGET} ${DRV_VERSION}")
message(STATUS     "---------------+-----------------------------------------------------------------------------------------------")
message(STATUS     " Target        | Description")
message(STATUS     "---------------+-----------------------------------------------------------------------------------------------")
message(STATUS     " kill          | Kills ${TARGET}")
message(STATUS     " update        | Calls following targets: clean -> install -> kill")
if(DEFINED ENV{PVSS_PROJ_PATH})
    message(STATUS " install       | Will install ${TARGET} ${PROJECT_VERSION} in: $ENV{PVSS_PROJ_PATH}/bin")
    message(STATUS "               |    + ${snap7_lib} alongside it")
    message(STATUS " run           | Runs ${TARGET} from ${CMAKE_BINARY_DIR} with:")
    message(STATUS "               |     ${TARGET} -num <driver_number> -proj <pvss_project_name> +config <driver_config_file>")
    message(STATUS "               |        pvss_project_name  = ${PVSS_PROJECT_NAME}")
    message(STATUS "               |        driver_number      = ${DRIVER_NUMBER}")
    message(STATUS "               |        driver_config_file = $ENV{PVSS_PROJ_PATH}/config/${DRIVER_CONFIG_FILE}")
else()
    message(STATUS " install       | PVSS_PROJ_PATH environment variable was not set. Cannot install from CMake.")
    message(STATUS " run      | PVSS_PROJ_PATH environment variable was not set. Cannot run local build from CMake.")
endif()
if(VALGRIND_PATH)
    message(STATUS " valgrind      | Runs ${TARGET} from ${CMAKE_BINARY_DIR} with valgrind.")
    message(STATUS "               |    (similar to run target)")
else()
    message(STATUS " valgrind      | Not available. valgrind or PVSS_PROJ_PATH not found.")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    message(STATUS " coverage      | Generates code coverage report (requires lcov)")
else()
    message(STATUS " coverage      | Not available. Build type is not Coverage -> -DCMAKE_BUILD_TYPE=Coverage. lcov required.")
endif()
message(STATUS     "---------------+-----------------------------------------------------------------------------------------------")
