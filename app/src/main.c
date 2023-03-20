#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/usb/usb_device.h>

#include "util.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

void main(void) {
	const struct device *dap_device = DEVICE_DT_GET(DT_NODELABEL(dap));
	FATAL_CHECK(device_is_ready(dap_device), "failed to ready dap interface");

	const struct device *io_device = DEVICE_DT_GET(DT_NODELABEL(io));
	FATAL_CHECK(device_is_ready(io_device), "failed to ready io interface");

	const struct device *vcp_device = DEVICE_DT_GET(DT_NODELABEL(vcp));
	FATAL_CHECK(device_is_ready(vcp_device), "failed to ready vcp interface");

	FATAL_CHECK(usb_enable(NULL) == 0, "failed to enable USB");

	struct net_if *iface = net_if_get_first_up();
	net_dhcpv4_start(iface);

	LOG_INF("Main Initialization Finished, Handling Requests Now!");
}
