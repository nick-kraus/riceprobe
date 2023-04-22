cmake_minimum_required(VERSION 3.20.0)
set(PROJECT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../")

find_package(Zephyr REQUIRED HINTS "${PROJECT_DIR}/zephyr")
project(test_nvs)

target_sources(app PRIVATE
    "src/main.c"
    "${PROJECT_DIR}/app/src/nvs.c"
)

target_include_directories(app PRIVATE
    "${PROJECT_DIR}/app/src"
)
