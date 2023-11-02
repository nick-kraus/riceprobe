cmake_minimum_required(VERSION 3.20.0)
set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../")

# native sim overlay
include("${CMAKE_CURRENT_LIST_DIR}/../boards/native_sim_64/native_sim_64.cmake")

find_package(Zephyr REQUIRED HINTS "${PROJECT_DIR}/firmware/modules/zephyr")
project(test_io)

target_sources(app PRIVATE
    "../boards/native_sim_64/pinctrl/pinctrl_sim.c"
    "src/io_transport.c"
    "src/main.c"
    "src/test_general.c"
    "src/test_pinctrl.c"
    "${PROJECT_DIR}/firmware/src/io/io.c"
    "${PROJECT_DIR}/firmware/src/io/commands_general.c"
    "${PROJECT_DIR}/firmware/src/io/commands_pinctrl.c"
    "${PROJECT_DIR}/firmware/src/nvs.c"
    "${PROJECT_DIR}/firmware/src/utf8.c"
)

target_include_directories(app PRIVATE
    "${PROJECT_DIR}/firmware/src"
    "${PROJECT_DIR}/firmware/tests"
)

target_compile_options(app PRIVATE -Wall -Werror -fanalyzer -save-temps=obj)

zephyr_linker_sources(DATA_SECTIONS
    "${PROJECT_DIR}/firmware/src/io/transport.ld"
)
