cmake_minimum_required(VERSION 3.20.0)
set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../")

# native sim overlay
include("${CMAKE_CURRENT_LIST_DIR}/../boards/native_sim_64/native_sim_64.cmake")

find_package(Zephyr REQUIRED HINTS "${PROJECT_DIR}/firmware/modules/zephyr")
project(test_nvs)

target_sources(app PRIVATE
    "../boards/native_sim_64/pinctrl/pinctrl_sim.c"
    "src/main.c"
    "${PROJECT_DIR}/firmware/src/nvs.c"
)

target_include_directories(app PRIVATE
    "${PROJECT_DIR}/firmware/src"
)

target_compile_options(app PRIVATE -Wall -Werror -fanalyzer)
