# must be set before importing the zephyr package
set(CONF_FILE "${CMAKE_CURRENT_LIST_DIR}/app.conf")
list(APPEND KCONFIG_ROOT "${CMAKE_CURRENT_LIST_DIR}/Kconfig")
list(APPEND BOARD_ROOT "${CMAKE_CURRENT_LIST_DIR}")
list(APPEND DTS_ROOT "${CMAKE_CURRENT_LIST_DIR}")

# for now just build for the main board we are developing for
set(BOARD rice_samv71b_xult)

find_package(Zephyr REQUIRED HINTS "${CMAKE_CURRENT_SOURCE_DIR}/zephyr")
enable_language(C)

target_sources(app PRIVATE
    "app/src/main.c"
    "app/src/nvs.c"
    "app/src/usb.c"
    "app/src/dap/dap.c"
    "app/src/dap/commands_general.c"
    "app/src/dap/commands_jtag_swd.c"
    "app/src/dap/usb.c"
    "app/src/io/io.c"
    "app/src/io/usb.c"
    "app/src/vcp/usb.c"
    "app/src/vcp/vcp.c"
)

target_include_directories(app PRIVATE
    "app/src"
)
