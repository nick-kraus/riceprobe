# get <pinctrl_soc.h> in the right path for all zephyr libraries
include_directories(SYSTEM "${CMAKE_CURRENT_LIST_DIR}/pinctrl/include")
# use custom native_sim devicetree overlay
set(DTC_OVERLAY_FILE "${CMAKE_CURRENT_LIST_DIR}/native_sim_64.overlay")
# allow pinctrl_sim.h defines in the devicetree, and custom driver bindings
list(APPEND DTS_ROOT
    "${PROJECT_DIR}/firmware"
    "${CMAKE_CURRENT_LIST_DIR}/pinctrl"
)
