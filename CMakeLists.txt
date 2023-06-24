cmake_minimum_required(VERSION 3.20.0)

# building for a bare-metal target, not an os
set(CMAKE_SYSTEM_NAME Generic)

# we will need to set languages after the zephyr package import, but languages none
# here will prevent cmake from trying to grab any host toolchains
project(RICEProbe LANGUAGES NONE)

# determine the repository version based off of git tags, and build
# this value into the firmware
execute_process(
    COMMAND git describe --always --long --dirty --match "v[0-9]*\.[0-9]*\.[0-9]*"
    OUTPUT_VARIABLE REPO_VERSION_STRING
)
string(STRIP ${REPO_VERSION_STRING} REPO_VERSION_STRING)
set(QUOTED_REPO_VERSION_STRING "\"${REPO_VERSION_STRING}\"")
if(NOT REPO_VERSION_STRING MATCHES "v[0-9]+.[0-9]+.[0-9]-[0-9]+-g[0-9a-fA-F]+")
    message(FATAL_ERROR "REPO_VERSION_STRING \"${REPO_VERSION_STRING}\" is not in a semver-compatible format")
else()
    message(STATUS "Repository version: ${REPO_VERSION_STRING}")
    string(REGEX REPLACE "\"v([0-9]+).([0-9]+).([0-9]+).+\"" "\\1" REPO_VERSION_MAJOR ${QUOTED_REPO_VERSION_STRING})
    string(REGEX REPLACE "\"v([0-9]+).([0-9]+).([0-9]+).+\"" "\\2" REPO_VERSION_MINOR ${QUOTED_REPO_VERSION_STRING})
    string(REGEX REPLACE "\"v([0-9]+).([0-9]+).([0-9]+).+\"" "\\3" REPO_VERSION_PATCH ${QUOTED_REPO_VERSION_STRING})
    set(CONFIG_REPO_VERSION_STRING ${QUOTED_REPO_VERSION_STRING} CACHE INTERNAL "repository version full string")
    set(CONFIG_REPO_VERSION_MAJOR ${REPO_VERSION_MAJOR} CACHE INTERNAL "repository version major integer")
    set(CONFIG_REPO_VERSION_MINOR ${REPO_VERSION_MINOR} CACHE INTERNAL "repository version minor integer")
    set(CONFIG_REPO_VERSION_PATCH ${REPO_VERSION_PATCH} CACHE INTERNAL "repository version patch integer")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/app/app.cmake")
