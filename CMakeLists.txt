cmake_minimum_required(VERSION 3.20.0)

# must be set before importing the zephyr package
set(CONF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/app/app.conf")
list(APPEND BOARD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/app")
list(APPEND DTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/app")

# for now just build for the main board we are developing for
set(BOARD rice_samv71b_xult)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rice)

add_subdirectory("app")
