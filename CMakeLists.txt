cmake_minimum_required(VERSION 3.20.0)

# must be set before importing the zephyr package
set(CONF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/app/app.conf")
list(APPEND BOARD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/app")
list(APPEND DTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/app")

# for now just build for the main board we are developing for
set(BOARD rice_samv71b_xult)

# determine the repository version based off of git tags, and build
# this value into the firmware
execute_process(
    COMMAND git describe --always --long --dirty --match "v[0-9]*\.[0-9]*\.[0-9]*"
    OUTPUT_VARIABLE REPO_VERSION_STRING
)
string(STRIP ${REPO_VERSION_STRING} REPO_VERSION_STRING)
if (NOT REPO_VERSION_STRING MATCHES "v[0-9]+.[0-9]+.[0-9]-[0-9]+-g[0-9a-fA-F]+")
    message(FATAL_ERROR "REPO_VERSION_STRING \"${REPO_VERSION_STRING}\" is not in a semver-compatible format")
else()
    string(REGEX REPLACE "v([0-9]+).([0-9]+).([0-9]+).+" "\\1" REPO_VERSION_MAJOR ${REPO_VERSION_STRING})
    string(REGEX REPLACE "v([0-9]+).([0-9]+).([0-9]+).+" "\\2" REPO_VERSION_MINOR ${REPO_VERSION_STRING})
    string(REGEX REPLACE "v([0-9]+).([0-9]+).([0-9]+).+" "\\3" REPO_VERSION_PATCH ${REPO_VERSION_STRING})
    message(STATUS "Repository version: ${REPO_VERSION_STRING}")
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rice)

add_subdirectory("app")
