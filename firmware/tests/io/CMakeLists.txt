cmake_minimum_required(VERSION 3.20.0)
set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../")

list(APPEND DTS_ROOT "${PROJECT_DIR}/firmware")

# used to get <pinctrl_soc.h> in the right path before all zephyr libraries
include_directories(SYSTEM "boards/pinctrl/include")
# used to allow pinctrl_posix.h defines in the devicetree
list(APPEND DTS_ROOT "${CMAKE_CURRENT_LIST_DIR}/boards/pinctrl")

find_package(Zephyr REQUIRED HINTS "${PROJECT_DIR}/firmware/modules/zephyr")
project(test_io)

target_sources(app PRIVATE
    "src/io_transport.c"
    "src/main.c"
    "src/test_general.c"
    "${PROJECT_DIR}/firmware/src/io/io.c"
    "${PROJECT_DIR}/firmware/src/io/commands_general.c"
    "${PROJECT_DIR}/firmware/src/nvs.c"
    "${PROJECT_DIR}/firmware/src/utf8.c"
)

target_include_directories(app PRIVATE
    "${PROJECT_DIR}/firmware/src"
    "${PROJECT_DIR}/firmware/tests"
)

target_compile_options(app PRIVATE -Wall -Werror -fanalyzer)

zephyr_linker_sources(DATA_SECTIONS
    "${PROJECT_DIR}/firmware/src/io/transport.ld"
)
