# must be set before importing the zephyr package
set(CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/app.conf")
list(APPEND KCONFIG_ROOT "${CMAKE_CURRENT_LIST_DIR}/Kconfig")
list(APPEND BOARD_ROOT "${CMAKE_CURRENT_LIST_DIR}")
list(APPEND DTS_ROOT "${CMAKE_CURRENT_LIST_DIR}")

# if no other board set, default to 'rice_samv71b_xult'
set(BOARD rice_samv71b_xult CACHE STRING "selected zephyr board")

# firmware output file name
set(CONFIG_KERNEL_BIN_NAME \"riceprobe_${REPO_VERSION_STRING}\" CACHE STRING "build output filename" FORCE)

find_package(Zephyr REQUIRED HINTS "${CMAKE_CURRENT_SOURCE_DIR}/zephyr")
enable_language(C)

target_sources(app PRIVATE
    "app/src/main.c"
    "app/src/nvs.c"
    "app/src/usb_msos.c"
    "app/src/dap/dap.c"
    "app/src/dap/commands_general.c"
    "app/src/dap/commands_jtag.c"
    "app/src/dap/commands_swd.c"
    "app/src/dap/commands_swo.c"
    "app/src/dap/commands_transfer.c"
    "app/src/dap/transport_tcp.c"
    "app/src/dap/transport_usb.c"
    "app/src/io/io.c"
    "app/src/io/usb.c"
    "app/src/vcp/usb.c"
    "app/src/vcp/vcp.c"
)

target_include_directories(app PRIVATE
    "app/src"
)

zephyr_linker_sources(DATA_SECTIONS
    "app/src/dap/transport.ld"
)
