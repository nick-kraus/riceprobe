cmake_minimum_required(VERSION 3.20.0)
set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../")

list(APPEND DTS_ROOT "${PROJECT_DIR}/firmware")

# used to get <pinctrl_soc.h> in the right path before all zephyr libraries
include_directories(SYSTEM "boards")

find_package(Zephyr REQUIRED HINTS "${PROJECT_DIR}/firmware/modules/zephyr")
project(test_dap)

target_sources(app PRIVATE
    "src/main.c"
    "src/test_general.c"
    "${PROJECT_DIR}/firmware/src/dap/dap.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_general.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_jtag.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_swd.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_swo.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_transfer.c"
    "${PROJECT_DIR}/firmware/src/nvs.c"
)

target_include_directories(app PRIVATE
    "${PROJECT_DIR}/firmware"
    "${PROJECT_DIR}/firmware/src"
)

zephyr_linker_sources(DATA_SECTIONS
    "${PROJECT_DIR}/firmware/src/dap/transport.ld"
)
