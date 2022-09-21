target_sources(app PRIVATE
    "src/main.c"
    "src/nvs.c"
    "src/usb.c"
    "src/dap/dap.c"
    "src/dap/commands_general.c"
    "src/dap/commands_jtag_swd.c"
    "src/dap/usb.c"
    "src/io/io.c"
    "src/io/usb.c"
    "src/vcp/usb.c"
    "src/vcp/vcp.c"
)

target_include_directories(app PRIVATE
    "src"
)

target_compile_definitions(app PRIVATE
    -DCONFIG_REPO_VERSION_STRING="${REPO_VERSION_STRING}"
    -DCONFIG_REPO_VERSION_MAJOR=${REPO_VERSION_MAJOR}
    -DCONFIG_REPO_VERSION_MINOR=${REPO_VERSION_MINOR}
    -DCONFIG_REPO_VERSION_PATCH=${REPO_VERSION_PATCH}
)
