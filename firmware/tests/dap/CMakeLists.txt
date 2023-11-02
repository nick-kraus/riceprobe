cmake_minimum_required(VERSION 3.20.0)
set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../")

# native sim overlay
include("${CMAKE_CURRENT_LIST_DIR}/../boards/native_sim_64/native_sim_64.cmake")

find_package(Zephyr REQUIRED HINTS "${PROJECT_DIR}/firmware/modules/zephyr")
project(test_dap)

target_sources(app PRIVATE
    "../boards/native_sim_64/pinctrl/pinctrl_sim.c"
    "src/dap_io.c"
    "src/dap_emul.c"
    "src/dap_transport.c"
    "src/main.c"
    "src/test_general.c"
    "src/test_jtag.c"
    "src/test_swd.c"
    "src/test_swo.c"
    "src/test_transfer.c"
    "${PROJECT_DIR}/firmware/src/dap/dap.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_general.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_jtag.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_swd.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_swo.c"
    "${PROJECT_DIR}/firmware/src/dap/commands_transfer.c"
    "${PROJECT_DIR}/firmware/src/nvs.c"
)

target_include_directories(app PRIVATE
    "${PROJECT_DIR}/firmware/src"
    "${PROJECT_DIR}/firmware/tests"
)

target_compile_options(app PRIVATE -Wall -Werror -fanalyzer -save-temps=obj)

zephyr_linker_sources(DATA_SECTIONS
    "${PROJECT_DIR}/firmware/src/dap/transport.ld"
)
