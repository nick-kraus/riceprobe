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
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
set(CONFIG_REPO_VERSION_STRING "\"${REPO_VERSION_STRING}\"" CACHE INTERNAL "repository version full string")

# must be set before importing the zephyr package
list(APPEND BOARD_ROOT "${CMAKE_CURRENT_LIST_DIR}")
list(APPEND DTS_ROOT "${CMAKE_CURRENT_LIST_DIR}")
# if no other board set, default to 'rice_samv71b_xult'
set(BOARD rice_samv71b_xult CACHE STRING "selected zephyr board")
# firmware output file name
set(CONFIG_KERNEL_BIN_NAME \"riceprobe_${REPO_VERSION_STRING}\" CACHE STRING "build output filename" FORCE)

find_package(Zephyr REQUIRED HINTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/zephyr")
enable_language(C)

target_sources(app PRIVATE
    "src/main.c"
    "src/nvs.c"
    "src/usb_msos.c"
    "src/dap/dap.c"
    "src/dap/commands_general.c"
    "src/dap/commands_jtag.c"
    "src/dap/commands_swd.c"
    "src/dap/commands_swo.c"
    "src/dap/commands_transfer.c"
    "src/dap/transport_tcp.c"
    "src/dap/transport_usb.c"
    "src/io/io.c"
    "src/io/usb.c"
    "src/vcp/usb.c"
    "src/vcp/vcp.c"
)

target_include_directories(app PRIVATE
    "src"
)

zephyr_linker_sources(DATA_SECTIONS
    "src/dap/transport.ld"
)
