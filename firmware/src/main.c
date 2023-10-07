#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/usb/usb_device.h>

#include "nvs.h"
#include "util.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

int32_t dap_init(void);

static int32_t early_init_ret = 0;
/* runs before main and many zephyr subsystems, to satisfy dependencies for those systems */
int32_t early_init(void) {
   if ((early_init_ret = nvs_init()) < 0) {
        LOG_ERR("nvs initialization failed with error %d", early_init_ret);
        return early_init_ret;
	}

	return 0;
}
/* may need to adjust level and priority based on what system dependencies are pre-empted */
SYS_INIT(early_init, POST_KERNEL, 80);

int32_t main(void) {
	if (early_init_ret < 0) { return early_init_ret; }

	int32_t ret;
	if ((ret = dap_init()) < 0) {
		LOG_ERR("dap initialization failed with error %d", ret);
		return ret;
	}

	const struct device *io_device = DEVICE_DT_GET(DT_NODELABEL(io));
	FATAL_CHECK(device_is_ready(io_device), "failed to ready io interface");

	const struct device *vcp_device = DEVICE_DT_GET(DT_NODELABEL(vcp));
	FATAL_CHECK(device_is_ready(vcp_device), "failed to ready vcp interface");

	FATAL_CHECK(usb_enable(NULL) == 0, "failed to enable USB");

	struct net_if *iface = net_if_get_first_up();
	net_dhcpv4_start(iface);

	LOG_INF("Main Initialization Finished, Handling Requests Now!");
	return 0;
}
