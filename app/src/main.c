#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/usb/usb_device.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

void main(void) {
	int32_t ret;

	const struct device *dap_device = DEVICE_DT_GET(DT_NODELABEL(dap));
	if (!device_is_ready(dap_device)) {
		LOG_ERR("failed to ready dap interface");
		return;
	}

	const struct device *io_device = DEVICE_DT_GET(DT_NODELABEL(io));
	if (!device_is_ready(io_device)) {
		LOG_ERR("failed to ready io interface");
		return;
	}

	const struct device *vcp_device = DEVICE_DT_GET(DT_NODELABEL(vcp));
	if (!device_is_ready(vcp_device)) {
		LOG_ERR("failed to ready vcp interface");
		return;
	}

	ret = usb_enable(NULL);
	if (ret != 0) {
		LOG_ERR("failed to enable USB");
		return;
	}

	LOG_INF("Main Initialization Finished, Handling USB Requests Now!");
}
