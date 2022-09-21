#include <drivers/gpio.h>
#include <logging/log.h>
#include <sys/ring_buffer.h>
#include <zephyr.h>

#include "dap/dap.h"
#include "dap/commands.h"

LOG_MODULE_DECLARE(dap);

int32_t dap_handle_command_swj_pins(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_swj_clock(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_swj_sequence(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_jtag_configure(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_jtag_sequence(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_jtag_idcode(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_swd_configure(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}

int32_t dap_handle_command_swd_sequence(const struct device *dev) {
    return -ENOTSUP; /* TODO */
}
