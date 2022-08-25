target_sources(app PRIVATE
    "src/main.c"
    "src/winusb.c"
    "src/dap/dap.c"
    "src/dap/usb.c"
    "src/io/io.c"
    "src/io/usb.c"
    "src/vcp/usb.c"
    "src/vcp/vcp.c"
)

target_include_directories(app PRIVATE
    "src"
)
