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
