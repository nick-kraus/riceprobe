cmake_minimum_required(VERSION 3.20.0)
set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../")

find_package(Zephyr REQUIRED HINTS "${PROJECT_DIR}/firmware/modules/zephyr")
project(test_utf8)

target_sources(app PRIVATE
    "src/main.c"
    "${PROJECT_DIR}/firmware/src/utf8.c"
)

target_include_directories(app PRIVATE
    "${PROJECT_DIR}/firmware/src"
)

target_compile_options(app PRIVATE -Wall -Werror -fanalyzer -save-temps=obj)
