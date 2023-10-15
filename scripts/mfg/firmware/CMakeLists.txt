cmake_minimum_required(VERSION 3.20.0)

# must be set before importing the zephyr package
set(CONF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mfg.conf")
list(APPEND BOARD_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../firmware")
list(APPEND DTS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../firmware")

# for now just build for the main board we are developing for
set(BOARD rice_samv71b_xult)
# include the project devicetree overlay
list(APPEND DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/mfg.overlay")

find_package(Zephyr REQUIRED HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../firmware/modules/zephyr")
project(mfg)

target_sources(app PRIVATE
    "src/main.c"
)
