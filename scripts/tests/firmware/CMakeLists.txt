cmake_minimum_required(VERSION 3.20.0)

# must be set before importing the zephyr package
set(CONF_FILE "${CMAKE_CURRENT_SOURCE_DIR}/target.conf")
# this is the only board supported by this firmware
set(BOARD nucleo_l4r5zi)
# adjustments to the board devicetree
list(APPEND DTC_OVERLAY_FILE "${CMAKE_CURRENT_SOURCE_DIR}/target.overlay")

find_package(Zephyr REQUIRED HINTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../zephyr")
project(target_firmware)

target_sources(app PRIVATE
    "src/main.c"
)
